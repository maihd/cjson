/******************************************************
 * C++ simple JSON parser
 *
 * @author: MaiHD
 * @license: Public domain
 * @copyright: MaiHD @ ${HOME}, 2018 - 2019
 ******************************************************/

#ifndef __JSON_H__
#define __JSON_H__

#define JSON_LIBNAME "libjson"
#define JSON_VERSION "v1.0.00"
#define JSON_VERCODE 10000

#ifndef JSON_API
#define JSON_API
#endif

#ifndef JSON_INLINE
#  if defined(_MSC_VER)
#     define JSON_INLINE __forceinline
#  elif defined(__cplusplus)
#     define JSON_INLINE inline
#  else
#     define JSON_INLINE 
#  endif
#endif

namespace json 
{
    /**
     * JSON type of json value
     */
    enum struct Type
    {
        Null,
        Array,
        Number,
        Object,
        String,
        Boolean,                               
    };

    /**
     * JSON error code
     */
    enum struct Error
    {
        None,
        
        /* Parsing error */

        WrongFormat,
        UnmatchToken,
        UnknownToken,
        UnexpectedToken,
        UnsupportedToken,

        /* Runtime error */

        OutOfMemory,
        InternalFailed,
    };

    struct State;
    struct Value;

    struct Settings
    {
        void* data;
        void* (*malloc)(void* data, size_t size);
        void  (*free)(void* data, void* pointer);
    };

    JSON_API const Value& parse(const char* json, State** state);
    JSON_API const Value& parse(const char* json, const Settings* settings, State** state);
    JSON_API void         release(State* state);
    JSON_API Error        get_errno(const State* state);
    JSON_API const char*  get_error(const State* state);

    /**
     * JSON value
     */
    struct Value
    {
    public: // @region: Fields
        Type type;
        union
        {
            double      number;
            bool        boolean;

            const char* string;

            Value** array;

            struct
            {
                const char* name;
                Value*      value;
            }* object;
        };

    public: // @region: Constants
        JSON_API static const Value NONE;

    public: // @region: Constructors
        JSON_INLINE Value()
            : type(Type::Null)
        {	
        }

        JSON_INLINE ~Value()
        {
            // SHOULD BE EMPTY
            // Memory are managed by State
        }

    public: // @region: Properties
        JSON_INLINE int length() const
        {
            switch (type)
            {
            case Type::Array:
                return array ? *((int*)array - 1) : 0;

            case Type::String:
                return string ? *((int*)string - 2) : 0;

            case Type::Object:
                return object ? *((int*)object - 1) : 0;

            default:
                return 0;
            }
        }
        
        JSON_API static bool  equals(const Value& a, const Value& b);
        JSON_API const Value& find(const char* name) const;

    public: // @region: Indexor
        JSON_INLINE const Value& operator[] (int index) const
        {
            if (type != Type::Array || index < 0 || index > this->length())
            {
                return NONE;
            }
            else
            {
                return *array[index];
            }	
        }

        JSON_INLINE const Value& operator[] (const char* name) const
        {
            return this->find(name);
        }

    public: // @region: Conversion
        JSON_INLINE operator const char* () const
        {
            return (this->type == Type::String) ? this->string : "";
        }

        JSON_INLINE operator double () const
        {
            return this->number;
        }

        JSON_INLINE operator bool () const
        {
            switch (type)
            {
            case Type::Number:
            case Type::Boolean:
            #ifdef NDEBUG
                return boolean;   // Faster, use when performance needed
            #else
                return !!boolean; // More precision, should use when debug
            #endif

            case Type::Array:
            case Type::Object:
                return true;

            case Type::String:
                return this->string && this->length() > 0;

            default: 
                return false;
            }
        }
    };

    JSON_INLINE bool operator==(const Value& a, const Value& b)
    {
        return Value::equals(a, b);
    }

    JSON_INLINE bool operator!=(const Value& a, const Value& b)
    {
        return !Value::equals(a, b);
    }
}

/* END OF __JSON_H__ */
#endif /* __JSON_H__ */