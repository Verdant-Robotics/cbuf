#pragma once
#include "cbuf_preamble.h"
#include <stdint.h> // uint8_t and such
#include <string.h> // memcpy
#include <vector>   // std::vector
#include <string>   // std::string
#include "vstring.h"



namespace cbufmsg {
    
    struct ATTR_PACKED metadata {
        // This has to be the first member
        mutable cbuf_preamble preamble = {
            CBUF_MAGIC,
            0,
            0xBE6738D544AB72C6,
            0.0,
            };
        uint64_t msg_hash;
        std::string msg_name;
        std::string msg_meta;
        /// This is here to ensure hash is always available, just in case.
        static const uint64_t TYPE_HASH = 0xBE6738D544AB72C6;
        static uint64_t hash() { return TYPE_HASH; }
        static constexpr const char* TYPE_STRING = "cbufmsg::metadata";
        static bool is_simple() { return false; }
        static bool supports_compact() { return false; }
        
        void Init()
        {
            msg_hash= 0;
            msg_name= "";
            msg_meta= "";
        }

        static void handle_metadata(cbuf_metadata_fn fn, void *ctx)
        {
            (*fn)(cbuf_string, hash(), TYPE_STRING, ctx);
        }

        size_t encode_size() const
        {
            return encode_net_size();
        }

        void free_encode(char *p) const
        {
            free(p);
        }

        bool encode(char *data, unsigned int buf_size) const
        {
            return encode_net(data, buf_size);
        }

        char *encode() const
        {
            size_t __struct_size = encode_size();
            preamble.setSize(uint32_t(__struct_size));
            char *data = reinterpret_cast<char *>(malloc(__struct_size));
            encode(data, __struct_size);
            return data;
        }

        bool decode(char *data, unsigned int buf_size)
        {
            return decode_net(data, buf_size);
        }

        size_t encode_net_size() const
        {
            size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)
            ret_size += sizeof(uint64_t); // for msg_hash
            ret_size += msg_name.length() + 4;
            ret_size += msg_meta.length() + 4;
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
            *reinterpret_cast<uint64_t *>(data) = this->msg_hash;
            data += sizeof(uint64_t);
            *reinterpret_cast<uint32_t *>(data) = uint32_t(this->msg_name.length());
            data += sizeof(uint32_t);
            memcpy(data, this->msg_name.c_str(), this->msg_name.length());
            data += this->msg_name.length();
            *reinterpret_cast<uint32_t *>(data) = uint32_t(this->msg_meta.length());
            data += sizeof(uint32_t);
            memcpy(data, this->msg_meta.c_str(), this->msg_meta.length());
            data += this->msg_meta.length();
            
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
            this->msg_hash = *reinterpret_cast<uint64_t *>(data);
            data += sizeof(uint64_t);
            uint32_t msg_name_length = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->msg_name.assign(data, msg_name_length);
            data += msg_name_length;
            uint32_t msg_meta_length = *reinterpret_cast<uint32_t *>(data);
            data += sizeof(uint32_t);
            this->msg_meta.assign(data, msg_meta_length);
            data += msg_meta_length;
            
            return true;
        }

        static constexpr const char * cbuf_string = R"CBUF_CODE(
namespace cbufmsg {
    struct metadata {
        uint64_t msg_hash;
        string msg_name;
        string msg_meta;
    }
}

)CBUF_CODE";

    };

};

