#ifndef __JSON_EX_H__
#define __JSON_EX_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void Json_print(const Json* value, FILE* out);
JSON_API void Json_write(const Json* value, FILE* out);
#endif /* __JSON_EX_H__ */

#ifdef JSON_EX_IMPL

#include "JsonEx.h"

/* @funcdef: Json_write */
void Json_write(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[");
            for (i = 0, n = value->length; i < n; i++)
            {
                Json_write(&value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JsonType_Object:
            fprintf(out, "{");
            for (i = 0, n = value->length; i < n; i++)
            {
                fprintf(out, "\"%s\" : ", value->object[i].name);
                Json_write(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }            
            }
            fprintf(out, "}");
            break;

        default:
            break;
        }
    }
}          

/* @funcdef: Json_print */
void Json_print(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;
        static int indent = 0;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                Json_print(&value->array[i], out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }  
            fputc(']', out);
            break;

        case JsonType_Object:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                fprintf(out, "\"%s\" : ", value->object[i].name);
                Json_print(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fputc('\n', out);
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {                    
                fputc(' ', out);
            }                    
            fputc('}', out);
            break;

        default:
            break;
        }
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_EX_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json* value, FILE* out);
JSON_API void JsonWrite(const Json* value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[");
            for (i = 0, n = value->length; i < n; i++)
            {
                JsonWrite(&value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JsonType_Object:
            fprintf(out, "{");
            for (i = 0, n = value->length; i < n; i++)
            {
                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonWrite(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }            
            }
            fprintf(out, "}");
            break;

        default:
            break;
        }
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;
        static int indent = 0;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                JsonPrint(&value->array[i], out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }  
            fputc(']', out);
            break;

        case JsonType_Object:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonPrint(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fputc('\n', out);
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {                    
                fputc(' ', out);
            }                    
            fputc('}', out);
            break;

        default:
            break;
        }
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json* value, FILE* out);
JSON_API void JsonWrite(const Json* value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[");
            for (i = 0, n = value->length; i < n; i++)
            {
                JsonWrite(&value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JsonType_Object:
            fprintf(out, "{");
            for (i = 0, n = value->length; i < n; i++)
            {
                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonWrite(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }            
            }
            fprintf(out, "}");
            break;

        default:
            break;
        }
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;
        static int indent = 0;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                JsonPrint(&value->array[i], out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }  
            fputc(']', out);
            break;

        case JsonType_Object:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonPrint(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fputc('\n', out);
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {                    
                fputc(' ', out);
            }                    
            fputc('}', out);
            break;

        default:
            break;
        }
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json* value, FILE* out);
JSON_API void JsonWrite(const Json* value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[");
            for (i = 0, n = value->length; i < n; i++)
            {
                JsonWrite(&value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JsonType_Object:
            fprintf(out, "{");
            for (i = 0, n = value->length; i < n; i++)
            {
                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonWrite(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }            
            }
            fprintf(out, "}");
            break;

        default:
            break;
        }
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;
        static int indent = 0;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                JsonPrint(&value->array[i], out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }  
            fputc(']', out);
            break;

        case JsonType_Object:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonPrint(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fputc('\n', out);
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {                    
                fputc(' ', out);
            }                    
            fputc('}', out);
            break;

        default:
            break;
        }
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json* value, FILE* out);
JSON_API void JsonWrite(const Json* value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[");
            for (i = 0, n = value->length; i < n; i++)
            {
                JsonWrite(&value->array[i], out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]");
            break;

        case JsonType_Object:
            fprintf(out, "{");
            for (i = 0, n = value->length; i < n; i++)
            {
                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonWrite(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fprintf(out, ",");
                }            
            }
            fprintf(out, "}");
            break;

        default:
            break;
        }
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;
        static int indent = 0;

        switch (value->type)
        {
        case JsonType_Null:
            fprintf(out, "null");
            break;

        case JsonType_Number:
            fprintf(out, "%lf", value->number);
            break;

        case JsonType_Boolean:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JsonType_String:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JsonType_Array:
            fprintf(out, "[\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                JsonPrint(&value->array[i], out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fprintf(out, "\n");
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {
                fprintf(out, " ");
            }  
            fputc(']', out);
            break;

        case JsonType_Object:
            fprintf(out, "{\n");

            indent++;
            for (i = 0, n = value->length; i < n; i++)
            {
                int j, m;
                for (j = 0, m = indent * 4; j < m; j++)
                {
                    fputc(' ', out);
                }

                fprintf(out, "\"%s\" : ", value->object[i].name);
                JsonPrint(&value->object[i].value, out);
                if (i < n - 1)
                {
                    fputc(',', out);
                }
                fputc('\n', out);
            }
            indent--;

            for (i = 0, n = indent * 4; i < n; i++)
            {                    
                fputc(' ', out);
            }                    
            fputc('}', out);
            break;

        default:
            break;
        }
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json value, FILE* out)
{
    int i, n;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[");
        for (i = 0, n = value.length; i < n; i++)
        {
            JsonWrite(value.array[i], out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "]");
        break;

    case JsonType_Object:
        fprintf(out, "{");
        for (i = 0, n = value.length; i < n; i++)
        {
            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonWrite(value.object[i].value, out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "}");
        break;

    default:
        break;
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json value, FILE* out)
{
    int i, n;
    static int indent = 0;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            JsonPrint(value.array[i], out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fprintf(out, "\n");
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fprintf(out, " ");
        }
        fputc(']', out);
        break;

    case JsonType_Object:
        fprintf(out, "{\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonPrint(value.object[i].value, out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fputc('\n', out);
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fputc(' ', out);
        }
        fputc('}', out);
        break;

    default:
        break;
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

//JSON_API int  Json_hash(const void* buffer, int length);

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json value, FILE* out)
{
    int i, n;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[");
        for (i = 0, n = value.length; i < n; i++)
        {
            JsonWrite(value.array[i], out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "]");
        break;

    case JsonType_Object:
        fprintf(out, "{");
        for (i = 0, n = value.length; i < n; i++)
        {
            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonWrite(value.object[i].value, out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "}");
        break;

    default:
        break;
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json value, FILE* out)
{
    int i, n;
    static int indent = 0;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            JsonPrint(value.array[i], out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fprintf(out, "\n");
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fprintf(out, " ");
        }
        fputc(']', out);
        break;

    case JsonType_Object:
        fprintf(out, "{\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonPrint(value.object[i].value, out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fputc('\n', out);
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fputc(' ', out);
        }
        fputc('}', out);
        break;

    default:
        break;
    }
}

#if 0
/* @funcdef: Json_hash */
int Json_hash(const void* buf, int len)
{
    int h = 0xdeadbeaf;

    const char* key = (const char*)buf;
    if (len > 3)
    {
        const int* key_x4 = (const int*)key;
        int i = len >> 2;
        do
        {
            int k = *key_x4++;

            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = (h * 5) + 0xe6546b64;
        } while (--i);

        key = (const char*)(key_x4);
    }

    if (len & 3)
    {
        int i = len & 3;
        int k = 0;

        key = &key[i - 1];
        do
        {
            k <<= 8;
            k |= *key--;
        } while (--i);

        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
#endif


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json value, FILE* out)
{
    int i, n;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[");
        for (i = 0, n = value.length; i < n; i++)
        {
            JsonWrite(value.array[i], out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "]");
        break;

    case JsonType_Object:
        fprintf(out, "{");
        for (i = 0, n = value.length; i < n; i++)
        {
            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonWrite(value.object[i].value, out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "}");
        break;

    default:
        break;
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json value, FILE* out)
{
    int i, n;
    static int indent = 0;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            JsonPrint(value.array[i], out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fprintf(out, "\n");
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fprintf(out, " ");
        }
        fputc(']', out);
        break;

    case JsonType_Object:
        fprintf(out, "{\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonPrint(value.object[i].value, out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fputc('\n', out);
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fputc(' ', out);
        }
        fputc('}', out);
        break;

    default:
        break;
    }
}


#endif /* JSON_UTILS_IMPL */

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__
#pragma once

#include "Json.h"
#include <stdio.h>

JSON_API void JsonPrint(const Json value, FILE* out);
JSON_API void JsonWrite(const Json value, FILE* out);
#endif /* __JSON_UTILS_H__ */

#ifdef JSON_UTILS_IMPL

#ifndef __JSON_UTILS_H__
#include "JsonUtils.h"
#endif // __JSON_UTILS_H__

/* @funcdef: Json_write */
void JsonWrite(const Json value, FILE* out)
{
    int i, n;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[");
        for (i = 0, n = value.length; i < n; i++)
        {
            JsonWrite(value.array[i], out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "]");
        break;

    case JsonType_Object:
        fprintf(out, "{");
        for (i = 0, n = value.length; i < n; i++)
        {
            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonWrite(value.object[i].value, out);
            if (i < n - 1)
            {
                fprintf(out, ",");
            }
        }
        fprintf(out, "}");
        break;

    default:
        break;
    }
}          

/* @funcdef: JsonPrint */
void JsonPrint(const Json value, FILE* out)
{
    int i, n;
    static int indent = 0;

    switch (value.type)
    {
    case JsonType_Null:
        fprintf(out, "null");
        break;

    case JsonType_Number:
        fprintf(out, "%lf", value.number);
        break;

    case JsonType_Boolean:
        fprintf(out, "%s", value.boolean ? "true" : "false");
        break;

    case JsonType_String:
        fprintf(out, "\"%s\"", value.string);
        break;

    case JsonType_Array:
        fprintf(out, "[\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            JsonPrint(value.array[i], out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fprintf(out, "\n");
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fprintf(out, " ");
        }
        fputc(']', out);
        break;

    case JsonType_Object:
        fprintf(out, "{\n");

        indent++;
        for (i = 0, n = value.length; i < n; i++)
        {
            int j, m;
            for (j = 0, m = indent * 4; j < m; j++)
            {
                fputc(' ', out);
            }

            fprintf(out, "\"%s\" : ", value.object[i].name);
            JsonPrint(value.object[i].value, out);
            if (i < n - 1)
            {
                fputc(',', out);
            }
            fputc('\n', out);
        }
        indent--;

        for (i = 0, n = indent * 4; i < n; i++)
        {
            fputc(' ', out);
        }
        fputc('}', out);
        break;

    default:
        break;
    }
}


#endif /* JSON_UTILS_IMPL */

