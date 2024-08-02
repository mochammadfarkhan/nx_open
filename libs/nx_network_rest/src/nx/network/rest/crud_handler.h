// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>
#include <utility>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/crud_model.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/type_traits.h>
#include <nx/utils/void.h>

#include "handler.h"
#include "json.h"
#include "params.h"
#include "request.h"
#include "response.h"

namespace nx::network::rest {

namespace detail {

#define MEMBER_CHECKER(MEMBER) \
    NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(DoesMemberExist_##MEMBER, MEMBER)
    MEMBER_CHECKER(front);
#undef MEMBER_CHECKER

template<typename Container>
decltype(auto) front(const Container& c)
{
    NX_ASSERT(c.size() != 0);
    if constexpr (nx::utils::IsKeyValueContainer<std::decay_t<Container>>::value)
        return c.begin()->second;
    else if constexpr (DoesMemberExist_front<std::decay_t<Container>>::value)
        return c.front();
    else
        return *c.begin();
}

template<typename... T>
constexpr bool StaticAssertAll()
{
    static_assert((T::value && ...));
    return (T::value && ...);
}

// Consider moving to `type_traits.h`. `FunctionArgumentType` is already declared in
// `signature_extractor.h`, but this one has a simpler interface: it allows to pass a pointer to
// the function directly.
template <auto FunctionPtr, size_t I>
using FunctionArgumentType = typename nx::utils::FunctionTraits<FunctionPtr>::template ArgumentType<I>;

} // namespace detail

enum class CrudFeature
{
    idInPost = 1 << 0, //< Allow providing id parameter in POST request.
    alwaysKeepDefault = 1 << 1, //< Keep default values in response regardless of _keepDefault.
    fastUpdate = 1 << 2, //< Execute update on PUT and PATCH without read.
};
Q_DECLARE_FLAGS(CrudFeatures, CrudFeature)

struct ResponseAttributes
{
    nx::network::http::HttpHeaders httpHeaders;
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
};

template<typename Derived>
class CrudHandler: public Handler
{
public:
    using Void = nx::utils::Void;

    explicit CrudHandler(QString idParamName = "id", CrudFeatures features = {}):
        // Maximum security if not set on registration.
        Handler(GlobalPermission::administrator, GlobalPermission::administrator),
        m_idParamName(std::move(idParamName)),
        m_features(features)
    {
    }

    explicit CrudHandler(CrudFeatures features): CrudHandler("id", features) {}

protected:
    virtual void modifyPathRouteResultOrThrow(PathRouter::Result* result) const override
    {
        if constexpr (DoesMethodExist_flexibleIdToId<Derived>::value)
        {
            const auto it = result->pathParams.findValue(m_idParamName);
            if (!it || (nx::Uuid::isUuidString(*it) && !nx::Uuid(*it).isNull()))
                return;
            const nx::Uuid id = static_cast<const Derived*>(this)->flexibleIdToId(*it);
            if (id.isNull())
                throw Exception::notFound(NX_FMT("Resource '%1' is not found", *it));
            result->pathParams.replace(m_idParamName, id.toString());
        }
    }

    // TODO:
    //   Consider moving this logic into the `Request` class.
    //   For example, Request's constructor can be injected with this validation logic.
    //   Request object should have maximum immutability.
    //   Handler should be agnostic of the actual URI path. It only expects particular parameters.
    virtual void clarifyApiVersionOrThrow(const PathRouter::Result& result, Request* request) const override
    {
        if (result.validationPath.startsWith("/rest/"))
        {
            auto name = result.validationPath.split('/')[2];
            if (!name.startsWith('v'))
                throw Exception::notFound(NX_FMT("API version '%1' is not supported", name));

            bool isOk = false;
            const auto number = name.mid(1).toInt(&isOk);
            if (!isOk || number <= 0)
                throw Exception::notFound(NX_FMT("API version '%1' is not supported", name));

            request->setApiVersion((size_t) number);
        }
    }

    virtual bool isConcreteIdProvidedInPath(PathRouter::Result* result) const override
    {
        auto it = result->pathParams.findValue(m_idParamName);
        return it && *it != NX_FMT("*");
    }

    #define METHOD_CHECKER(METHOD) \
        NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(DoesMethodExist_##METHOD, METHOD)
    METHOD_CHECKER(loadFromParams);
    METHOD_CHECKER(size);
    METHOD_CHECKER(toParams);
    METHOD_CHECKER(create);
    METHOD_CHECKER(read);
    METHOD_CHECKER(delete_);
    METHOD_CHECKER(update);
    METHOD_CHECKER(addSubscription);
    METHOD_CHECKER(getSubscriptionId);
    METHOD_CHECKER(flexibleIdToId);
    METHOD_CHECKER(fillMissingParams);
    METHOD_CHECKER(fillMissingParamsForResponse);
    #undef METHOD_CHECKER

    virtual Response executeGet(const Request& request) override;
    virtual Response executePatch(const Request& request) override;
    virtual Response executePost(const Request& request) override;
    virtual Response executePut(const Request& request) override;
    virtual Response executeDelete(const Request& request) override;
    virtual QString subscriptionId(const Request& request) override;
    virtual nx::utils::Guard subscribe(
        const QString& id, const Request& request, SubscriptionCallback callback) override;
    virtual QString idParamName() const override { return m_idParamName; }

    template<typename OutputData>
    QByteArray serializeOrThrow(const OutputData& outputData, Qn::SerializationFormat format)
    {
        if (format == Qn::SerializationFormat::urlEncoded
            || format == Qn::SerializationFormat::urlQuery)
        {
            if (!outputData.isObject())
            {
                throw nx::network::rest::Exception::unsupportedMediaType(
                    NX_FMT("Unsupported format for non-object data: %1", format));
            }

            const auto encode = format == Qn::SerializationFormat::urlEncoded
                ? QUrl::FullyEncoded
                : QUrl::PrettyDecoded;
            return Params::fromJson(outputData.toObject()).toUrlQuery().query(encode).toUtf8();
        }
        auto serializedData = trySerialize(outputData, format);
        if (!serializedData.has_value())
        {
            throw nx::network::rest::Exception::unsupportedMediaType(
                NX_FMT("Unsupported format: %1", format));
        }
        return *serializedData;
    }

    template<typename T>
    T deserialize(const Request& request, bool wrapInObject = false)
    {
        T data;
        if constexpr (DoesMethodExist_loadFromParams<T>::value)
        {
            data.loadFromParams(static_cast<Derived*>(this)->resourcePool(), request.params());
            if (!data.isValid())
                throw Exception::badRequest(NX_FMT("Invalid: %1", typeid(T)));
            if (!request.params().contains("local"))
                data.isLocal = request.isLocal();
            if (!request.params().contains("format"))
                data.format = request.responseFormatOrThrow();
        }
        else if constexpr (!std::is_same<T, Void>::value)
        {
            data = request.parseContentOrThrow<T>(wrapInObject);
        }
        return data;
    }

    template<typename T>
    struct ArgumentCount;

    template<typename Result, typename Object, typename... Args>
    struct ArgumentCount<Result(Object::*)(Args...)>
    {
        static constexpr auto value = sizeof...(Args);
    };

    template<typename Method, typename Model>
    auto call(Method method, Model model, const Request& request, ResponseAttributes* responseAttributes)
    {
        auto d = static_cast<Derived*>(this);
        if constexpr (ArgumentCount<Method>::value == 3)
            return (d->*method)(std::move(model), request, responseAttributes);
        else
            return (d->*method)(std::move(model), request);
    }

    template<typename Data>
    Response response(
        Data&& data, const Request& request, ResponseAttributes responseAttributes = {},
        Params filters = {}, json::DefaultValueAction defaultValueAction = json::DefaultValueAction::appendMissing)
    {
        auto json = json::filter(std::forward<Data>(data), std::move(filters), defaultValueAction);
        if (NX_ASSERT(m_schemas))
            m_schemas->postprocessResponse(request, &json);
        const auto format = request.responseFormatOrThrow();
        Response response(responseAttributes.statusCode, std::move(responseAttributes.httpHeaders));
        response.content = {
            {Qn::serializationFormatToHttpContentType(format)}, serializeOrThrow(json, format)};
        return response;
    }

    template<typename Id>
    Response responseById(Id id, const Request& request, ResponseAttributes responseAttributes = {})
    {
        if (auto resource = readById(id, request, &responseAttributes))
        {
            if constexpr (DoesMethodExist_fillMissingParamsForResponse<Derived>::value)
                static_cast<Derived*>(this)->fillMissingParamsForResponse(&*resource, request);
            return response(std::move(*resource), request, std::move(responseAttributes));
        }

        // This may actually happen if created resource is removed faster than the handler
        // generated a response.
        throw Exception::internalServerError(NX_FMT("Resource '%1' is not found", id));
    }

    template<typename Id>
    auto readById(Id id, const Request& request, ResponseAttributes* responseAttributes)
    {
        using detail::front;
        auto list = call(&Derived::read, id, request, responseAttributes);
        using ResultType = std::optional<std::decay_t<decltype(front(list))>>;
        if (list.empty())
            return ResultType(std::nullopt);
        if (const auto size = list.size(); size != 1)
        {
            const auto error =
                NX_FMT("There are %1 resources with '%2' id while it should be one.", size, id);
            NX_ASSERT(false, error);
            throw Exception::internalServerError(error);
        }
        return ResultType(std::move(front(list)));
    }

    struct IdParam
    {
        std::optional<QString> value;
        bool isInPath = false;
    };

    IdParam idParam(const Request& request) const
    {
        const auto& params = request.params();
        const auto id = params.findValue(m_idParamName);

        if (!id || !request.isConcreteIdProvided())
            return {};

        return {id, request.pathParams().contains(m_idParamName)};
    }

    bool extractKeepDefault(Params* params, bool keepDefaultFeature, bool defaultBehavior)
    {
        const auto extractValue =
            [&params](const QString& name) -> std::optional<bool>
            {
                const auto value = params->findValue(name);
                if (!value)
                    return std::nullopt;
                params->remove(name);
                return (*value != "false" && *value != "0");
            };

        const auto keepDefault = extractValue("_keepDefault");
        const auto stripDefault = extractValue("_stripDefault");
        if (keepDefaultFeature)
            return true; //< And ignore stripped options.
        if (!keepDefault)
            return stripDefault ? !*stripDefault : defaultBehavior;
        if (stripDefault)
            throw Exception::badRequest("Only one can be used: _keepDefault, _stripDefault");
        return keepDefault.value_or(defaultBehavior);
    }

    json::DefaultValueAction extractDefaultValueAction(
        Params* params, std::optional<size_t> apiVersion) const
    {
        const auto extractBoolParam =
            [&](const QString& name) -> std::optional<bool>
            {
                const auto value = params->findValue(name);
                if (!value)
                    return std::nullopt;
                params->remove(name);
                return (*value != "false" && *value != "0");
            };

        const auto extractBoolValue =
            [&](bool defaultBehavior) -> bool
            {
                const auto keepDefault = extractBoolParam("_keepDefault");
                const auto stripDefault = extractBoolParam("_stripDefault");
                if (m_features.testFlag(CrudFeature::alwaysKeepDefault))
                    return true; //< And ignore extracted values.
                if (!keepDefault)
                    return stripDefault ? !*stripDefault : defaultBehavior;
                if (stripDefault)
                    throw Exception::badRequest("Only one can be used: _keepDefault, _stripDefault");
                return keepDefault.value_or(defaultBehavior);
            };

        return extractBoolValue(/*defaultBehavior*/ apiVersion.value_or(0) >= 3)
            ? json::DefaultValueAction::appendMissing
            : json::DefaultValueAction::removeEqual;
    }

    template<typename Filter>
    std::tuple<std::decay_t<Filter>, Params, json::DefaultValueAction> processGetRequest(
        const Request& request)
    {
        using FilterType = std::decay_t<Filter>;

        FilterType filter = request.params().toJson(/*excludeCommon*/ true).isEmpty()
            ? FilterType()
            : deserialize<FilterType>(request);
        Params filtered;

        if constexpr (DoesMethodExist_toParams<FilterType>::value)
        {
            filtered = filter.toParams();
            filtered.insert("local", QString());
            filtered.insert("format", QString());
            filtered.insert("extraFormatting", QString());
        }
        else
        {
            if (auto intermediate = json::serialized(filter); intermediate.isObject())
                filtered = Params::fromJson(intermediate.toObject());
        }

        auto params = request.params();
        for (auto [key, value]: filtered.keyValueRange())
            params.remove(key);
        auto defaultValueAction = extractDefaultValueAction(&params, request.apiVersion());

        return {std::move(filter), std::move(params), defaultValueAction};
    }

protected:
    const QString m_idParamName;
    const CrudFeatures m_features;
};

template<typename Derived>
Response CrudHandler<Derived>::executeGet(const Request& request)
{
    if constexpr (DoesMethodExist_read<Derived>::value)
    {
        auto [filter, params, defaultValueAction] =
            processGetRequest<detail::FunctionArgumentType<&Derived::read, 0>>(request);
        ResponseAttributes responseAttributes;
        auto list = call(&Derived::read, std::move(filter), request, &responseAttributes);
        if constexpr (DoesMethodExist_fillMissingParamsForResponse<Derived>::value)
        {
            auto* d = static_cast<Derived*>(this);
            for (auto& model: list)
                d->fillMissingParamsForResponse(&model, request);
        }
        if (const auto id = idParam(request);
            (id.value && id.isInPath) || m_idParamName.isEmpty())
        {
            using detail::front;
            const auto size = list.size();
            if (size == 1)
            {
                return response(std::move(front(list)),
                    request, std::move(responseAttributes), std::move(params), defaultValueAction);
            }

            if (size == 0)
            {
                if (id.value)
                    throw Exception::notFound(NX_FMT("Resource '%1' is not found", *(id.value)));
                else
                    throw Exception::notFound("Resource is not found");
            }
            const auto error = NX_FMT("There are %1 resources while it should be one.", size);
            NX_DEBUG(this, error);
            if (id.value)
                throw Exception::invalidParameter(*id.value, error);
            else
                throw Exception::internalServerError(error);
        }
        return response(std::move(list),
            request, std::move(responseAttributes), std::move(params), defaultValueAction);
    }
    else
    {
        throw nx::network::rest::Exception::notAllowed("This endpoint does not allow GET");
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executePost(const Request& request)
{
    using namespace nx::utils::model;

    if constexpr (DoesMethodExist_create<Derived>::value)
    {
        using Model = detail::FunctionArgumentType<&Derived::create, 0>;
        const auto derived = static_cast<Derived*>(this);
        if (!m_features.testFlag(CrudFeature::idInPost) && idParam(request).value)
        {
            throw Exception::badRequest(
                NX_FMT("The parameter %1 must not be specified when creating a new object.",
                    m_idParamName));
        }
        Model model = deserialize<Model>(request);
        if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
            derived->fillMissingParams(&model, request);

        if constexpr (isIdGenerationEnabled<Model>)
        {
            // This will reference the actual Model type in errors. Grep for `StaticAssertAll`.
            detail::StaticAssertAll<HasGetId<Model>, CanSetIdWithArg<Model, nx::Uuid>>();
            if (auto id = getId(model); id == decltype(id)())
            {
                setId(model, nx::Uuid::createUuid());
            }
            else if constexpr (DoesMethodExist_read<Derived>::value)
            {
                ResponseAttributes responseAttributes;
                // This is pretty wasteful, since the idea here is just to check for existence.
                // TODO: `virtual bool checkExisting(const nx::Uuid&) const { /*use resource pool*/ }
                if (call(&Derived::read, std::move(id), request, &responseAttributes).size() == 1)
                    throw Exception::forbidden("Already exists");
            }
        }

        ResponseAttributes responseAttributes;
        using Result = typename nx::utils::FunctionTraits<&Derived::create>::ReturnType;
        if constexpr (!std::is_same<Result, void>::value)
        {
            auto result = call(&Derived::create, std::move(model), request, &responseAttributes);
            return response(std::move(result), request, std::move(responseAttributes));
        }
        else if constexpr (DoesMethodExist_read<Derived>::value
            && HasGetId<Model>::value)
        {
            auto id = getId(model);
            call(&Derived::create, std::move(model), request, &responseAttributes);
            return responseById(std::move(id), request, std::move(responseAttributes));
        }
        else
        {
            call(&Derived::create, std::move(model), request, &responseAttributes);
            return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
        }
    }
    else
    {
        throw nx::network::rest::Exception::notAllowed("This endpoint does not allow POST");
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executeDelete(const Request& request)
{
    using nx::utils::model::getId;

    if constexpr (DoesMethodExist_delete_<Derived>::value)
    {
        ResponseAttributes responseAttributes;
        using Id = detail::FunctionArgumentType<&Derived::delete_, 0>;
        if constexpr (std::is_same<Id, Void>::value)
        {
            call(&Derived::delete_, Void(), request, &responseAttributes);
        }
        else
        {
            Id id = deserialize<Id>(request);
            if constexpr (nx::utils::model::adHocIsIdCheckOnDeleteEnabled<Id>
                && !DoesMethodExist_loadFromParams<Id>::value)
            {
                if (getId(id) == std::decay_t<decltype(getId(id))>())
                    throw Exception::missingParameter(m_idParamName);
            }
            call(&Derived::delete_, std::move(id), request, &responseAttributes);
        }
        return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
    }
    else
    {
        throw nx::network::rest::Exception::notAllowed("This endpoint does not allow DELETE");
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executePut(const Request& request)
{
    using namespace nx::utils::model;

    if constexpr (DoesMethodExist_update<Derived>::value)
    {
        const auto d = static_cast<Derived*>(this);
        using Model = detail::FunctionArgumentType<&Derived::update, 0>;
        using Result = typename nx::utils::FunctionTraits<&Derived::update>::ReturnType;
        Model model = deserialize<Model>(request, /*wrapInObject*/ true);
        if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
            d->fillMissingParams(&model, request);
        ResponseAttributes responseAttributes;
        if (idParam(request).value || m_idParamName.isEmpty())
        {
            if constexpr (!std::is_same<Result, void>::value)
            {
                using detail::front;
                auto result =
                    call(&Derived::update, std::move(model), request, &responseAttributes);
                if constexpr (DoesMethodExist_size<Result>::value)
                {
                    if (NX_ASSERT(result.size() == 1, "Expected 1 result, got %1", result.size()))
                        return response(std::move(front(result)), request, std::move(responseAttributes));
                    return {
                        responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
                }
                else
                {
                    return response(std::move(result), request, std::move(responseAttributes));
                }
            }
            else
            {
                if constexpr (DoesMethodExist_read<Derived>::value)
                {
                    using IdType = std::conditional_t<
                        HasGetId<Model>::value,
                        typename HasGetId<Model>::type,
                        std::monostate
                    >;
                    IdType id = {};

                    if constexpr (HasGetId<Model>::value)
                    {
                        id = getId(model);
                        if (id == IdType{})
                            throw Exception::missingParameter(m_idParamName);
                    }

                    call(&Derived::update, std::move(model), request, &responseAttributes);
                    if constexpr (HasGetId<Model>::value)
                    {
                        if (!m_features.testFlag(CrudFeature::fastUpdate))
                        {
                            return responseById(
                                id, request, std::move(responseAttributes));
                        }
                    }
                }
                else
                {
                    call(&Derived::update, std::move(model), request, &responseAttributes);
                }

                return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
            }
        }

        if constexpr (!std::is_same<Result, void>::value)
        {
            auto result = call(&Derived::update, std::move(model), request, &responseAttributes);
            return response(std::move(result), request, std::move(responseAttributes));
        }
        else
        {
            call(&Derived::update, std::move(model), request, &responseAttributes);
            if constexpr (DoesMethodExist_read<Derived>::value)
            {
                if (!m_features.testFlag(CrudFeature::fastUpdate))
                {
                    auto emptyFilter = detail::FunctionArgumentType<&Derived::read, 0>{};
                    auto result =
                        call(&Derived::read, std::move(emptyFilter), request, &responseAttributes);
                    return response(std::move(result), request, std::move(responseAttributes));
                }
            }
            return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
        }
    }
    else
    {
        throw nx::network::rest::Exception::notAllowed("This endpoint does not allow PUT");
    }
}

template<typename Derived>
Response CrudHandler<Derived>::executePatch(const Request& request)
{
    using namespace nx::utils::model;

    if constexpr (DoesMethodExist_read<Derived>::value && DoesMethodExist_update<Derived>::value)
    {
        const auto d = static_cast<Derived*>(this);
        using Model = detail::FunctionArgumentType<&Derived::update, 0>;
        using Result = typename nx::utils::FunctionTraits<&Derived::update>::ReturnType;
        std::optional<QJsonValue> incomplete;
        Model model = request.parseContentAllowingOmittedValuesOrThrow<Model>(&incomplete);
        ResponseAttributes responseAttributes;
        if (idParam(request).value || m_idParamName.isEmpty())
        {
            if constexpr (HasGetId<Model>::value)
            {
                if (incomplete && !m_features.testFlag(CrudFeature::fastUpdate))
                {
                    auto id = getId(model);
                    if (id == decltype(id)())
                        throw Exception::missingParameter(m_idParamName);

                    const auto systemAccessGuard = request.forceSystemAccess();
                    const auto existing = readById(id, request, &responseAttributes);
                    if (!existing)
                        throw Exception::notFound(NX_FMT("Resource '%1' is not found", id));
                    QString error;
                    if (!json::merge(
                        &model, *existing, *incomplete, &error, /*chronoSerializedAsDouble*/ true))
                    {
                        throw Exception::badRequest(error);
                    }
                }
            }

            if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
                d->fillMissingParams(&model, request);
            if constexpr (!std::is_same_v<Result, void>)
            {
                auto result =
                    call(&Derived::update, std::move(model), request, &responseAttributes);
                return response(std::move(result), request, std::move(responseAttributes));
            }
            else
            {
                call(&Derived::update, std::move(model), request, &responseAttributes);

                if constexpr (HasGetId<Model>::value)
                {
                    if (!m_features.testFlag(CrudFeature::fastUpdate))
                        return responseById(getId(model), request, std::move(responseAttributes));
                }
                return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
            }
        }

        if constexpr (HasGetId<Model>::value)
        {
            if (!m_features.testFlag(CrudFeature::fastUpdate))
            {
                auto emptyFilter = getId(model);
                if (!incomplete)
                {
                    QJsonValue serialized;
                    QJson::serialize(model, &serialized);
                    incomplete = std::move(serialized);
                }
                const auto systemAccessGuard = request.forceSystemAccess();
                QString error;
                if (!json::merge(
                    &model,
                    call(&Derived::read, emptyFilter, request, &responseAttributes),
                    *incomplete,
                    &error,
                    /*chronoSerializedAsDouble*/ true))
                {
                    throw Exception::badRequest(error);
                }
            }
        }
        if constexpr (DoesMethodExist_fillMissingParams<Derived>::value)
            d->fillMissingParams(&model, request);

        if constexpr (!std::is_same<Result, void>::value)
        {
            auto result =
                call(&Derived::update, std::move(model), request, &responseAttributes);
            return response(std::move(result), request, std::move(responseAttributes));
        }
        else
        {
            call(&Derived::update, std::move(model), request, &responseAttributes);
            if constexpr (HasGetId<Model>::value)
            {
                if (!m_features.testFlag(CrudFeature::fastUpdate))
                {
                    const auto result = call(&Derived::read, getId(model), request, &responseAttributes);
                    return response(result, request, std::move(responseAttributes));
                }
            }
            return {responseAttributes.statusCode, std::move(responseAttributes.httpHeaders)};
        }
    }
    else
    {
        throw nx::network::rest::Exception::notAllowed("This endpoint does not allow PATCH");
    }
}

template<typename Derived>
nx::utils::Guard CrudHandler<Derived>::subscribe(
    const QString& id, const Request&, SubscriptionCallback callback)
{
    NX_ASSERT(!id.isEmpty(), "Id must be obtained by subscriptionId()");
    if constexpr (DoesMethodExist_addSubscription<Derived>::value)
    {
        return static_cast<Derived*>(this)->addSubscription(id, std::move(callback));
    }
    else
    {
        NX_ASSERT(
            false, "Check that handler supports subscriptions using subscriptionId().isNull()");
        return {};
    }
}

template<typename Derived>
QString CrudHandler<Derived>::subscriptionId(const Request& request)
{
    if constexpr (DoesMethodExist_getSubscriptionId<Derived>::value)
        return static_cast<Derived*>(this)->getSubscriptionId(request);
    else
        return {};
}

} // namespace nx::network::rest
