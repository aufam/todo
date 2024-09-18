#include <jwt-cpp/jwt.h>
#include <delameta/error.h>

using namespace Project;
using delameta::Result;
using etl::Ok;
using etl::Err;
using namespace std::literals;

const auto EXPIRES_IN = 60min * 24;
const auto ISSUER = "auth0";
const auto ALGORITHM = jwt::algorithm::hs256{"secret"};

[[export]]
auto jwt_create_token(const std::vector<std::pair<std::string, std::string>>& payload) -> std::string {
    auto j = jwt::create()
        .set_type("JWT")
        .set_issuer(ISSUER)
        .set_expires_at(std::chrono::system_clock::now() + EXPIRES_IN);

    for (const auto& [key, value]: payload) {
        j.set_payload_claim(key, jwt::claim(value));
    }

    return j.sign(ALGORITHM);
}

[[export, throw]]
auto jwt_decode_token(const std::string& token) -> Result<std::string> {
    try {
        auto decoded = jwt::decode(token);
        jwt::verify()
            .allow_algorithm(ALGORITHM)
            .with_issuer(ISSUER)
            .verify(decoded);

        auto exp_claim = decoded.get_expires_at();
        if (exp_claim <= std::chrono::system_clock::now()) {
            throw std::runtime_error("Token has expired");
        }

        return Ok(decoded.get_payload());
    } catch (const std::exception& e) {
        return Err(e.what());
    }
}
