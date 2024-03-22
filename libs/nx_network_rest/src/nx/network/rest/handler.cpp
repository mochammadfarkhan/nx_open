// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "handler.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/abstract_socket.h>
#include <nx/utils/log/log.h>

namespace nx::network::rest {

void Handler::validateAndAmend(Request* request, http::HttpHeaders* headers)
{
    if (NX_ASSERT(m_schemas) && request->method() != nx::network::http::Method::options)
        m_schemas->validateOrThrow(request, headers);
}

Response Handler::executeAnyMethod(const Request& request)
{
    // Default implementation should respond with `notAllowed` if a Method is not defined by the
    // OpenAPI schema.
    //
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/501
    // > If the server does recognize the method, but intentionally does not support it, the
    // appropriate response is 405 Method Not Allowed.
    //
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/405
    // > The HyperText Transfer Protocol (HTTP) 405 Method Not Allowed response status code
    // indicates that the server knows the request method, **but the target resource doesn't
    // support this method**. [emphasis added]

    if (request.method() == nx::network::http::Method::get)
        return executeGet(request);
    if (request.method() == nx::network::http::Method::post)
        return executePost(request);
    if (request.method() == nx::network::http::Method::put)
        return executePut(request);
    if (request.method() == nx::network::http::Method::patch)
        return executePatch(request);
    if (request.method() == nx::network::http::Method::delete_)
        return executeDelete(request);
    if (request.method() == nx::network::http::Method::options)
        return {nx::network::http::StatusCode::ok};
    return {nx::network::http::StatusCode::notAllowed};
}

Response Handler::executeGet(const Request& /*request*/)
{
    return {nx::network::http::StatusCode::notAllowed};
}

Response Handler::executeDelete(const Request& /*request*/)
{
    return {nx::network::http::StatusCode::notAllowed};
}

Response Handler::executePost(const Request& /*request*/)
{
    return {nx::network::http::StatusCode::notAllowed};
}

Response Handler::executePut(const Request& /*request*/)
{
    return {nx::network::http::StatusCode::notAllowed};
}

Response Handler::executePatch(const Request& /*request*/)
{
    return {nx::network::http::StatusCode::notAllowed};
}

Handler::Handler(GlobalPermission readPermissions, GlobalPermission modifyPermissions):
    m_readPermissions(readPermissions),
    m_modifyPermissions(modifyPermissions)
{
}

Response Handler::executeRequestOrThrow(Request* request, http::HttpHeaders* headers)
{
    if (!NX_ASSERT(request))
        throw Exception::internalServerError("Request is nullptr");

    validateAndAmend(request, headers);
    Response response = executeAnyMethod(*request);
    if (response.content && response.content->type.value == http::header::ContentType::kJson.value
        && request->isExtraFormattingRequired())
    {
        response.content->body = nx::utils::formatJsonString(response.content->body);
    }
    return response;
}

void Handler::afterExecute(
    const Request& /*request*/,
    const Response& response,
    std::unique_ptr<AbstractStreamSocket> socket)
{
    NX_ASSERT(!response.isUndefinedContentLength || socket);
}

Handler::GlobalPermission Handler::readPermissions() const
{
    return m_readPermissions;
}

void Handler::setReadPermissions(GlobalPermission permissions)
{
    m_readPermissions = permissions;
}

Handler::GlobalPermission Handler::modifyPermissions() const
{
    return m_modifyPermissions;
}

void Handler::setModifyPermissions(GlobalPermission permissions)
{
    m_modifyPermissions = permissions;
}

void Handler::setPath(const QString& path)
{
    m_path = path;
}

void Handler::setSchemas(std::shared_ptr<json::OpenApiSchemas> schemas)
{
    m_schemas = std::move(schemas);
}

QStringList Handler::cameraIdUrlParams() const
{
    return {};
}

std::string_view Handler::corsMethodsAllowed(const nx::network::http::Method& method) const
{
    if (method == nx::network::http::Method::get || method == nx::network::http::Method::options)
        return "GET, OPTIONS";
    else
        return {};
}

QString Handler::extractAction(const QString& path) const
{
    QString localPath = path;
    while(localPath.endsWith('/'))
        localPath.chop(1);
    return localPath.mid(localPath.lastIndexOf('/') + 1);
}

} // namespace nx::network::rest
