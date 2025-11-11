#include "HttpClient.hpp"

HttpClient::HttpClient() {}

const char* httpMethodToString(esp_http_client_method_t method)
{
    switch (method)
    {
        case HTTP_METHOD_GET:
            return "HTTP_GET";
        case HTTP_METHOD_POST:
            return "HTTP_POST";
        case HTTP_METHOD_PUT:
            return "HTTP_PUT";
        case HTTP_METHOD_PATCH:
            return "HTTP_PATCH";
        case HTTP_METHOD_DELETE:
            return "HTTP_DELETE";
        case HTTP_METHOD_HEAD:
            return "HTTP_HEAD";
        case HTTP_METHOD_NOTIFY:
            return "HTTP_NOTIFY";
        case HTTP_METHOD_SUBSCRIBE:
            return "HTTP_SUBSCRIBE";
        case HTTP_METHOD_UNSUBSCRIBE:
            return "HTTP_UNSUBSCRIBE";
        case HTTP_METHOD_OPTIONS:
            return "HTTP_OPTIONS";
        case HTTP_METHOD_COPY:
            return "HTTP_COPY";
        case HTTP_METHOD_MOVE:
            return "HTTP_MOVE";
        case HTTP_METHOD_LOCK:
            return "HTTP_LOCK";
        case HTTP_METHOD_UNLOCK:
            return "HTTP_UNLOCK";
        case HTTP_METHOD_PROPFIND:
            return "HTTP_PROPFIND";
        case HTTP_METHOD_PROPPATCH:
            return "HTTP_PROPPATCH";
        case HTTP_METHOD_MKCOL:
            return "HTTP_MKCOL";
        case HTTP_METHOD_REPORT:
            return "HTTP_REPORT";
        case HTTP_METHOD_MAX:
            return "HTTP_MAX";
        default:
            return "HTTP_UNKNOWN";
    }
}
bool HttpClient::isJson(const std::string& str)
{
    return (!str.empty() && ((str[0] == '{' && str[str.length() - 1] == '}') ||
                             (str[0] == '[' && str[str.length() - 1] == ']')));
}

// Perform GET request and return parsed JSON
std::unique_ptr<json> HttpClient::get(const std::string& url, const std::string& header_key,
                                      const std::string& header_value)
{
    return performRequest(HTTP_METHOD_GET, url, header_key, header_value, "", "", "", false);
}

std::unique_ptr<json> HttpClient::post(const std::string& url, const std::string& header_key,
                                       const std::string& header_value, std::string body,
                                       const std::string& user, const std::string& password,
                                       bool ignore_response)
{
    return performRequest(HTTP_METHOD_POST, url, header_key, header_value, body, user, password,
                          ignore_response);
}

std::unique_ptr<json> HttpClient::put(const std::string& url, const std::string& header_key,
                                      const std::string& header_value, std::string body,
                                      bool ignore_response)
{
    return performRequest(HTTP_METHOD_PUT, url, header_key, header_value, body, "", "",
                          ignore_response);
}

std::unique_ptr<json> HttpClient::performRequest(esp_http_client_method_t method,
                                                 const std::string&       url,
                                                 const std::string&       header_key,
                                                 const std::string&       header_value,
                                                 const std::string& body, const std::string& user,
                                                 const std::string& password, bool ignore_response)
{
    std::unique_ptr<std::string> output_buffer = performRequestRaw(
        method, url, header_key, header_value, body, user, password, ignore_response);

    if (!output_buffer)
    {
        return nullptr;
    }

    if (!isJson(*output_buffer))
    {
        if (ignore_response)
        {
            return nullptr;
        }
        ESP_LOGE(TAG, "Response is not valid JSON: %s", output_buffer->c_str());
        return nullptr;
    }

    // parse the json response.
    nlohmann::json parsed_json = nlohmann::json::parse(*output_buffer, nullptr, false);

    if (parsed_json.is_discarded())
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", output_buffer->c_str());
        return nullptr;
    }

    return std::make_unique<json>(std::move(parsed_json));
}

std::unique_ptr<std::string>
HttpClient::performRequestRaw(esp_http_client_method_t method, const std::string& url,
                              const std::string& header_key, const std::string& header_value,
                              const std::string& body, const std::string& user,
                              const std::string& password, bool ignore_response)
{
    esp_err_t                    err;
    std::unique_ptr<std::string> output_buffer = std::make_unique<std::string>();
    esp_http_client_config_t     config        = {};
    memset(&config, 0, sizeof(config));
    config.url                         = url.c_str();
    config.timeout_ms                  = 10000;
    config.skip_cert_common_name_check = true; // disable CN check

    if (!output_buffer)
    {
        return nullptr;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // select method
    if (method != HTTP_METHOD_POST && method != HTTP_METHOD_PUT && method != HTTP_METHOD_GET)
    {
        ESP_LOGE(TAG, "Unsupported HTTP method");
        esp_http_client_cleanup(client);
        return nullptr;
    }
    err = esp_http_client_set_method(client, method);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set HTTP method: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return nullptr;
    }

    // set header
    if (!header_key.empty() && !header_value.empty())
    {
        err = esp_http_client_set_header(client, header_key.c_str(), header_value.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP header: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return nullptr;
        }
    }
    else
    {
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    }

    // set username and password
    if (!user.empty())
    {
        err = esp_http_client_set_username(client, user.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set HTTP username: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return nullptr;
        }
        esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);

        // set password if provided
        if (!password.empty())
        {
            err = esp_http_client_set_password(client, password.c_str());
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to set HTTP password: %s", esp_err_to_name(err));
                esp_http_client_cleanup(client);
                return nullptr;
            }
        }
    }

    // open connection
    err = esp_http_client_open(client, body.length());
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return nullptr;
    }

    // send body
    if (!body.empty())
    {
        int wlen = esp_http_client_write(client, body.c_str(), body.length());
        if (wlen < 0)
        {
            ESP_LOGE(TAG, "Write failed");
            esp_http_client_cleanup(client);
            return nullptr;
        }
    }

    // get response length
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0)
    {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        esp_http_client_cleanup(client);
        return nullptr;
    }

    // read response
    if (content_length > 0)
    {
        output_buffer->resize(content_length);
        int data_read =
            esp_http_client_read_response(client, output_buffer->data(), output_buffer->size());
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Failed to read response");
            esp_http_client_cleanup(client);
            return nullptr;
        }

        ESP_LOGI(TAG, "HTTP %s Status = %d, content_length = %" PRId64, httpMethodToString(method),
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }

    // cleanup http transaction
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (output_buffer->empty())
    {
        if (!ignore_response)
            ESP_LOGW(TAG, "No response body received");
        return nullptr;
    }

    return output_buffer;
}