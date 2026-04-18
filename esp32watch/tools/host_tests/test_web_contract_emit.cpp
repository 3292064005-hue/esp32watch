#include <cstdio>
#include "Arduino.h"
#include "web/web_contract_json.h"

int main() {
    String response("{");
    response += "\"ok\":true,";
    web_contract_append_json(response);
    response += "}";
    std::fputs(response.c_str(), stdout);
    return 0;
}
