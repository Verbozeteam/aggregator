#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

#include <unordered_map>

std::vector<char> create_param(std::vector<std::string> headers, std::vector<char> data) {
    std::vector<char> ret;
    for (int i = 0; i < headers.size(); i++) {
        ret.insert(std::end(ret), std::begin(headers[i]), std::end(headers[i]));
        ret.push_back('\r'); ret.push_back('\n');
    }
    ret.push_back('\r'); ret.push_back('\n');
    ret.insert(std::end(ret), std::begin(data), std::end(data));
    return ret;
}

std::vector<char> param_string(std::string name, std::string value) {
    return create_param(
        {"content-disposition: form-data; name=\"" + name + "\""},
        {std::vector<char>(value.begin(), value.end())}
    );
}

std::vector<char> param_int(std::string name, int value) {
    return param_string(name, std::to_string(value));
}

std::vector<char> param_json(std::string name, json value) {
    std::string value_string = value.dump();
    return create_param(
        {"content-disposition: form-data; name=\"" + name + "\"",
         "content-type: application/json"},
        {std::vector<char>(value_string.begin(), value_string.end())}
    );
}

std::vector<char> param_file(std::string name, std::string filename, std::vector<char> contents) {
    return create_param(
        {"content-disposition: form-data; name=\"" + name + "\"; filename=\"" + filename + "\""},
        {contents}
    );
}

std::vector<char> param_file(std::string name, std::string filename, std::string mime_type, std::vector<char> contents) {
    return create_param(
        {"content-disposition: form-data; name=\"" + name + "\"; filename=\"" + filename + "\"",
         "content-type: " + mime_type},
        {contents}
    );
}

/** contains information about a sent (in progress) http request */
struct SENT_HTTP_REQUEST {
    struct lws* lws_client;
    std::vector<std::pair<std::string, std::string>> headers; // <header-name, header-value> pairs
    std::vector<char> body;
    size_t cur_body_index;
    VerbozeHttpResponse response;
    HttpResponseCallback callback;

    SENT_HTTP_REQUEST() :
        lws_client(nullptr),
        body(),
        cur_body_index(0),
        callback(nullptr) {}
};

std::vector<SENT_HTTP_REQUEST*> g_pending_http_requests;

SENT_HTTP_REQUEST* get_sent_http_request(struct lws* client) {
    for (int i = 0; i < g_pending_http_requests.size(); i++) {
        if (g_pending_http_requests[i]->lws_client == client)
            return g_pending_http_requests[i];
    }
    return nullptr;
}

void delete_sent_http_request(struct lws* client) {
    for (int i = 0; i < g_pending_http_requests.size(); i++) {
        if (g_pending_http_requests[i]->lws_client == client) {
            delete g_pending_http_requests[i];
            g_pending_http_requests.erase(g_pending_http_requests.begin() + i);
            break;
        }
    }
}

static std::string concat_url(std::string u1, std::string u2) {
    if (u1.size() == 0)
        return u2;
    else if (u2.size() == 0)
        return u1;

    if (u1.at(u1.size()-1) == '/')
        u1 = u1.substr(0, u1.size()-1);
    if (u2.at(0) == '/')
        u2 = u2.substr(1);
    return u1 + "/" + u2;
}

static std::string make_request_url(std::string path) {
    return concat_url(ConfigManager::get<std::string>("http-protocol")+"://" + ConfigManager::get<std::string>("verboze-url"), path);
}

static int connect_http_client(
        std::string token,
        std::string method,
        std::string url,
        std::vector<std::pair<std::string, std::string>> headers,
        std::vector<std::vector<char>> params,
        HttpResponseCallback callback) {
    int port = 80;
    bool is_ssl = false;
    std::string path = "";
    std::string address = "";

    std::string new_url = url;

    if (new_url.find("http://") == 0) {
        new_url = new_url.substr(7);
    } else if (new_url.find("https://") == 0) {
        new_url = new_url.substr(8);
        is_ssl = true;
        port = 443;
    }
    int slash_index = (int)new_url.find("/");
    if (slash_index != (int)std::string::npos) {
        path = new_url.substr(slash_index);
        new_url = new_url.substr(0, slash_index);
    }
    int colon_index = (int)new_url.find(":");
    if (colon_index != (int)std::string::npos) {
        try {
            std::string port_str = new_url.substr(colon_index+1);
            port = std::stoi(port_str);
        } catch (...) {
            LOG(error) << "Failed to read port in URL " << url;
        }
        new_url = new_url.substr(0, colon_index);
    }

    address = new_url;

    LOG(info) << "HTTP connecting to " << (is_ssl ? "https://" : "http://") << address << ":" << port << path;

    char boundary_buf[64];
    lws_snprintf(boundary_buf, 63, "%08x%08x%08x", rand(), rand(), rand());
    std::string boundary = boundary_buf;

    SENT_HTTP_REQUEST* req = new SENT_HTTP_REQUEST;
    req->response.url = url;
    req->callback = callback;
    for (int i = 0; i < headers.size(); i++)
        req->headers.push_back(std::pair<std::string, std::string>(headers[i].first+":", headers[i].second));
    req->headers.push_back(std::pair<std::string, std::string>("content-type:", "multipart/form-data; boundary="+boundary));
    if (token.size() > 0)
        req->headers.push_back(std::pair<std::string, std::string>("authorization:", "token " + token));
    for (int i = 0; i < params.size(); i++) {
        std::string boundary_line = "--" + boundary + "\r\n";
        req->body.insert(std::end(req->body), std::begin(boundary_line), std::end(boundary_line));
        req->body.insert(std::end(req->body), std::begin(params[i]), std::end(params[i]));
        req->body.push_back('\r'); req->body.push_back('\n');
    }
    if (params.size() > 0) {
        std::string boundary_line = "--" + boundary + "--";
        req->body.insert(std::end(req->body), std::begin(boundary_line), std::end(boundary_line));
    }
    g_pending_http_requests.push_back(req);

	struct lws_client_connect_info info;
    memset(&info, 0, sizeof(struct lws_client_connect_info));
	info.context = VerbozeAPI::GetLWSContext();
	info.port = port;
	info.address = address.c_str();
	info.path = path.c_str();
	info.host = info.address;
	info.origin = info.address;
	info.method = method.c_str();
	info.ssl_connection = !is_ssl ? 0 :
        LCCSCF_USE_SSL |
        LCCSCF_ALLOW_SELFSIGNED |
        LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

	info.protocol = "http-broker";
	info.pwsi = &req->lws_client;

    int ret = !lws_client_connect_via_info(&info);

    if (ret != 0) {
        for (int i = 0; i < g_pending_http_requests.size(); i++) {
            if (g_pending_http_requests[i] == req) {
                g_pending_http_requests.erase(g_pending_http_requests.begin() + i);
                break;
            }
        }
        delete req;
    }

    return ret;
}

int http_callback_broker(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    // find a SENT_HTTP_REQUEST that corresponds to this wsi
    SENT_HTTP_REQUEST* request = get_sent_http_request(wsi);
	char buf[LWS_PRE + 1024];
    char* p = &buf[LWS_PRE];
	int n;

	switch (reason) {
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		LOG(error) << "HTTP CLIENT_CONNECTION_ERROR: " << (in ? (char *)in : "(null)");
        if (request && !request->response.status_code)
            request->response.status_code = 500;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
        if (!request) {
            LOG(error) << "Closing HTTP client but no SENT_HTTP_REQUEST was found for it";
            return 1;
        } else {
            if (request->callback)
                request->callback(request->response);
            delete_sent_http_request(wsi);
        }
		break;

	case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
        request->response.status_code = lws_http_client_http_response(wsi);
		break;

	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
        if (!request) {
            LOG(error) << "Received data on HTTP client but no SENT_HTTP_REQUEST was found for it";
            return 1;
        } else {
            size_t prev_size = request->response.raw_data.size();
            request->response.raw_data.resize(prev_size + len);
            for (int i = 0; i < len; i++)
                request->response.raw_data[prev_size+i] = ((char*)in)[i];
        }
		return 0;

	case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
		n = sizeof(buf) - LWS_PRE;
		if (lws_http_client_read(wsi, &p, &n) < 0)
			return -1;
		return 0;

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
        if (!request) {
            LOG(error) << "Completed HTTP client but no SENT_HTTP_REQUEST was found for it";
            return 1;
        } else {
            request->response.raw_data.push_back(0);
            try {
                request->response.data = json::parse(std::string(request->response.raw_data.data(), request->response.raw_data.size()-1));
            } catch (...) {}
            if (request->callback) {
                request->callback(request->response);
                request->callback = nullptr;
            }
        }
		break;

	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
        if (!request) {
            LOG(error) << "Handshake started for HTTP client but no SENT_HTTP_REQUEST was found for it";
            return 1;
        } else {
            uint8_t* buffer_end = (*(uint8_t**)in) + len - 1;
            for (int i = 0; i < request->headers.size(); i++) {
                std::string name = request->headers[i].first;
                std::string value = request->headers[i].second;
                if (lws_add_http_header_by_name(wsi,
                        (uint8_t *)name.c_str(),
                        (uint8_t *)value.c_str(), value.size(),
                        (uint8_t**)in, buffer_end)) {
                    LOG(error) << "Failed to append HTTP header " << name << ":" << value;
                    return 1;
                }
            }

            if (request->body.size() > 0) {
                if (lws_add_http_header_content_length(wsi, request->body.size(), (uint8_t **)in, buffer_end)) {
                    LOG(error) << "Failed to append HTTP content-length";
                    return 1;
                }
                lws_client_http_body_pending(wsi, 1);
                lws_callback_on_writable(wsi);
            }
        }

		break;

	case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
        if (!request) {
            LOG(error) << "HTTP client writable but no SENT_HTTP_REQUEST was found for it";
            return 1;
        } else {
            if (request->body.size() > request->cur_body_index) {
                int num_bytes = request->cur_body_index + 200 >= request->body.size() ? request->body.size() - request->cur_body_index : 200;
                lws_write_protocol n = request->body.size() == request->cur_body_index + num_bytes ? LWS_WRITE_HTTP_FINAL : LWS_WRITE_HTTP;
                if (n == LWS_WRITE_HTTP_FINAL)
			        lws_client_http_body_pending(wsi, 0);
                if (lws_write(wsi, (uint8_t*)&request->body[request->cur_body_index], num_bytes, n) != num_bytes) {
                    LOG(error) << "Failed to write to HTTP connection";
                    return 1;
                }
                if (n != LWS_WRITE_HTTP_FINAL)
        			lws_callback_on_writable(wsi);
                request->cur_body_index += num_bytes;
            }
        }
        return 0;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

void VerbozeAPI::Endpoints::DefaultResponseHandler(VerbozeHttpResponse response) {
    std::string log = "HTTP " + response.url + " (" + std::to_string(response.status_code) + "): ";

    if (!response.data.is_null())
        log += response.data.dump(4);
    else if (response.raw_data.size())
        log += response.raw_data.data();

    if (response.status_code >= 200 && response.status_code < 300)
        LOG(trace) << log;
    else
        LOG(warning) << log;
}

void VerbozeAPI::Endpoints::RegisterRoom(
    std::string room_id,
    std::string room_name,
    std::string interface,
    std::string ip,
    int port,
    int type,
    std::string data,
    HttpResponseCallback callback) {

    connect_http_client(m_connection_token, "POST", make_request_url("api/rooms/"), {}, {
            param_string("identifier", room_id),
            param_string("room_name", room_name),
            param_string("interface", interface),
            param_string("ip", ip),
            param_int("port", port),
            param_int("type", type),
            param_string("data", data),
        }, callback);
}
