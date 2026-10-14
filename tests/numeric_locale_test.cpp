// CD Player - JSON restoration with the same process-local locale as PS5.
// Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include <clocale>
#include <cstdio>
#include <fstream>
#include <iterator>
int main(){
    const auto *locale=std::localeconv();
    if(!locale || !locale->decimal_point || std::string(locale->decimal_point)!=".")return 1;
    std::ifstream input("tests/fixtures/settings.json");
    const std::string bytes((std::istreambuf_iterator<char>(input)),{});
    if(bytes.empty())return 2;
    const auto expected=nlohmann::json::parse(bytes,nullptr,false);
    if(!expected.is_object())return 3;
    for(int i=0;i<1000;i++){
        auto restored=nlohmann::json::parse(bytes,nullptr,false);
        if(restored!=expected || nlohmann::json::parse(restored.dump(),nullptr,false)!=expected)return 4;
    }
    if(!nlohmann::json::parse("{broken",nullptr,false).is_discarded())return 5;
    std::puts("C locale: valid; saved preference fixture parsed and round-tripped 1000 times; malformed JSON handled");
    return 0;
}
