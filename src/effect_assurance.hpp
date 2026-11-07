#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum class EffectAssurance { UNAVAILABLE, DISPATCH_CONFIRMED, CORRELATED, EXACT, USER_CONFIRMED };
inline const char* assuranceName(EffectAssurance value) {
    switch(value) {
        case EffectAssurance::EXACT:return "EXACT";
        case EffectAssurance::CORRELATED:return "CORRELATED";
        case EffectAssurance::DISPATCH_CONFIRMED:return "DISPATCH_CONFIRMED";
        case EffectAssurance::USER_CONFIRMED:return "USER_CONFIRMED";
        default:return "UNAVAILABLE";
    }
}
inline EffectAssurance assuranceFrom(const std::string& value) {
    for(auto level:{EffectAssurance::EXACT,EffectAssurance::CORRELATED,EffectAssurance::DISPATCH_CONFIRMED,EffectAssurance::USER_CONFIRMED})if(value==assuranceName(level))return level;
    return EffectAssurance::UNAVAILABLE;
}
inline bool acceptsAssurance(EffectAssurance actual,const nlohmann::json& contract) {
    // Human attestation is a separate evidence source, never a machine tier.
    if(actual==EffectAssurance::USER_CONFIRMED)return contract.value("allow_user_confirmation",false);
    const auto minimum=assuranceFrom(contract.value("minimum_assurance",std::string("CORRELATED")));
    // Explicit machine-policy matrix. Human evidence is deliberately separate.
    switch(minimum) {
        case EffectAssurance::DISPATCH_CONFIRMED:return actual==EffectAssurance::DISPATCH_CONFIRMED || actual==EffectAssurance::CORRELATED || actual==EffectAssurance::EXACT;
        case EffectAssurance::CORRELATED:return actual==EffectAssurance::CORRELATED || actual==EffectAssurance::EXACT;
        case EffectAssurance::EXACT:return actual==EffectAssurance::EXACT;
        default:return false;
    }
}
struct EffectReport {
    std::string effectId,stepId,source;
    EffectAssurance assurance=EffectAssurance::UNAVAILABLE;
};
