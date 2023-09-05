#pragma once
#include <hjson.h>
#include <toolbox/hjson_helper.h> // this header has json helpers
// Please include all the required cbuf C headers before this file
#include "inctype.h"
template<typename T>
void loadFromJson(const Hjson::Value& json, T&obj);


#if !defined(_JSON_DECLARATION_inctype_)
#define _JSON_DECLARATION_inctype_
template<>
  void loadFromJson(const Hjson::Value& json, messages::inctype& obj);
#endif // _JSON_DECLARATION_inctype_



#if !defined(_JSON_IMPLEMENTATION_inctype_)
#define _JSON_IMPLEMENTATION_inctype_
template<>
inline void loadFromJson(const Hjson::Value& json, messages::inctype& obj)
{
    do { // Loading sensor_name
        {
            std::string tmp;
            if (get_member_string(json, "sensor_name", tmp)) {
                obj.sensor_name = tmp;
            }
        }
    } while(0);
    do { // Loading val
        get_member_uint(json, "val", obj.val);
    } while(0);
}
#endif // _JSON_IMPLEMENTATION_inctype_



