// A small asynchronous HTTP server on Boost.Beast, following the shape of
// Beast's own example/http/server/async: one listener accepting connections,
// one session per connection, all on a shared io_context run by N threads.
//
// Listens on 0.0.0.0:$PORT (read at runtime, default 8080) and serves at the
// root path:
//   GET /        -> a plain-text greeting
//   GET /health  -> {"status":"ok"}, the fleet's health check
//   anything else -> 404

#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Build the response for one request.
static http::response<http::string_body> handle_request(const http::request<http::string_body>& req)
{
    auto make = [&](http::status status, std::string content_type, std::string body) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::move(body);
        res.prepare_payload();
        return res;
    };

    if (req.method() != http::verb::get && req.method() != http::verb::head)
        return make(http::status::method_not_allowed, "text/plain", "Method not allowed\n");

    const std::string target(req.target());
    if (target == "/")
        return make(http::status::ok, "text/plain", "Hello from the Boost.Beast template!\n");
    if (target == "/health")
        return make(http::status::ok, "application/json", "{\"status\":\"ok\"}");
    return make(http::status::not_found, "text/plain", "Not found\n");
}

// One HTTP connection.
class session : public std::enable_shared_from_this<session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;

public:
    explicit session(tcp::socket&& socket) : stream_(std::move(socket)) {}

    void run()
    {
        net::dispatch(stream_.get_executor(), beast::bind_front_handler(&session::do_read, shared_from_this()));
    }

private:
    void do_read()
    {
        req_ = {};
        stream_.expires_after(std::chrono::seconds(30));
        http::async_read(stream_, buffer_, req_, beast::bind_front_handler(&session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t)
    {
        if (ec == http::error::end_of_stream)
            return do_close();
        if (ec)
            return;

        auto res = std::make_shared<http::response<http::string_body>>(handle_request(req_));
        http::async_write(stream_, *res,
            [self = shared_from_this(), res](beast::error_code ec, std::size_t) {
                if (ec)
                    return;
                if (!res->keep_alive())
                    return self->do_close();
                self->do_read();
            });
    }

    void do_close()
    {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }
};

// Accepts incoming connections and launches the sessions.
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(net::make_strand(ioc))
    {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
    }

    void run() { do_accept(); }

private:
    void do_accept()
    {
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (!ec)
            std::make_shared<session>(std::move(socket))->run();
        do_accept();
    }
};

int main()
{
    const char* env_port = std::getenv("PORT");
    const int port_num = env_port ? std::atoi(env_port) : 8080;
    const auto port = static_cast<unsigned short>(port_num > 0 && port_num < 65536 ? port_num : 8080);
    const int threads = std::max(1u, std::min(4u, std::thread::hardware_concurrency()));

    net::io_context ioc{threads};
    try {
        std::make_shared<listener>(ioc, tcp::endpoint{net::ip::make_address("0.0.0.0"), port})->run();
    } catch (const std::exception& e) {
        std::cerr << "Server failed to listen on port " << port << ": " << e.what() << std::endl;
        return 1;
    }

    // Stop cleanly on SIGINT / SIGTERM (docker stop).
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](beast::error_code, int) { ioc.stop(); });

    std::cout << "Listening on 0.0.0.0:" << port << std::endl;

    std::vector<std::thread> pool;
    for (int i = 1; i < threads; ++i)
        pool.emplace_back([&ioc] { ioc.run(); });
    ioc.run();
    for (auto& t : pool)
        t.join();
    return 0;
}
