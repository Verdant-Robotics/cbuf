#pragma once
#include "cbuf_preamble.h"
#include <stdint.h> // uint8_t and such
#include <string.h> // memcpy
#include <vector>   // std::vector
#include <string>   // std::string
#include "vstring.h"



namespace messages {
    constexpr const char * CSTRING = "This is a const string";
    
    struct ATTR_PACKED inctype {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            sizeof(inctype),
            0x48F259C269692BE8,
            0.0,
            };
        VString<15> sensor_name = "somename";
        uint32_t val = 10;
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0x48F259C269692BE8;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "messages::inctype";
        static bool is_simple() { return true; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            sensor_name = "somename";
            val = 10;
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
        }

        size_t encode_size() const
        {
            return sizeof(inctype);
        }

        void free_encode(const char *) const {}

        bool encode(char *data, unsigned int buf_size) const
        {
            if (buf_size < sizeof(inctype)) return false;
            memcpy(data, this, sizeof(*this));
            return true;
        }

        // This variant allows for no copy when writing to disk.
        const char *encode() const
        {
            return reinterpret_cast<const char *>(this);
        }

        bool decode(char *data, unsigned int buf_size)
        {
            if (buf_size < sizeof(inctype)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            memcpy(this, data, sizeof(*this));
            return true;
        }

        static bool decode(char *data, unsigned int buf_size, inctype** var)
        {
            if (buf_size < sizeof(inctype)) return false;
            cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);
            if (pre->hash != TYPE_HASH) return false;
            *var = reinterpret_cast<inctype *>(data);
            return true;
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += sizeof(VString<15>); // for sensor_name
            ret_size += sizeof(uint32_t); // for val
            return ret_size;
        }

        bool encode_net(char *data, unsigned int buf_size) const
        {
            preamble.setSize(uint32_t(encode_net_size()));
            if (buf_size < preamble.size()) return false;
            *reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint32_t *>(data) = preamble.size_;
            data += sizeof(uint32_t);
            *reinterpret_cast<uint64_t *>(data) = preamble.hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<double *>(data) = preamble.packet_timest;
            data += sizeof(double);
            *reinterpret_cast<VString<15> *>(data) = this->sensor_name;
            data += sizeof(VString<15>);
            *reinterpret_cast<uint32_t *>(data) = this->val;
            data += sizeof(uint32_t);
            
            return true;
        }

        bool decode_net(char *data, unsigned int buf_size)
        {
            uint32_t magic = *reinterpret_cast<uint32_t *>(data);
            if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();
            preamble.magic = magic;
            data += sizeof(uint32_t);
            uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);
            preamble.size_ = dec_size;
            if (preamble.size() > buf_size) CBUF_RETURN_FALSE();
            data += sizeof(uint32_t);
            uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);
            if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();
            data += sizeof(uint64_t);
            preamble.packet_timest = *reinterpret_cast<double *>(data);
            data += sizeof(double);
            this->sensor_name = *reinterpret_cast<VString<15> *>(data);
            data += sizeof(VString<15>);
            this->val = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace messages {
    struct inctype {
        short_string sensor_name = "somename";
        uint32_t val = 10;
    }
}

)CBUF_CODE";

    };

};

