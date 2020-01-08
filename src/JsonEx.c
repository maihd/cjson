#include "JsonEx.h"

/* @funcdef: JsonWrite */
void JsonWrite(const Json* value, FILE* out)
{
    if (value)
    {
        int i, n;

        switch (value->type)
        {
        case JSON_NULL:
            fprintf(out, "null");
            break;

        case JSON_NUMBER:
            fprintf(out, "%lf", value->number);
            break;

        case JSON_BOOLEAN:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JSON_STRING:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JSON_ARRAY:
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

        case JSON_OBJECT:
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

        case JSON_NONE:
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
        case JSON_NULL:
            fprintf(out, "null");
            break;

        case JSON_NUMBER:
            fprintf(out, "%lf", value->number);
            break;

        case JSON_BOOLEAN:
            fprintf(out, "%s", value->boolean ? "true" : "false");
            break;

        case JSON_STRING:
            fprintf(out, "\"%s\"", value->string);
            break;

        case JSON_ARRAY:
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

        case JSON_OBJECT:
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

        case JSON_NONE:
        default:
            break;
        }
    }
}

static void* Json_TempAlloc(JsonTempAllocator* data, int size)
{
    if (data->marker + size <= data->length)
    {
        void* result  = (char*)data->buffer + data->marker;
        data->marker += size;
        return result;
    }
    
    return NULL;
}

static void* Json_TempFree(void* data, void* ptr)
{
    (void)data;
    (void)ptr;
}

bool JsonTempAllocator_Init(JsonTempAllocator* allocator, void* buffer, int length)
{
    if (buffer && length > 0)
    {
        allocator->super.data   = allocator;
        allocator->super.free   = Json_TempFree;
        allocator->super.alloc  = (void*(*)(void*, int))Json_TempAlloc;
        allocator->buffer       = buffer;
        allocator->length       = length;
        allocator->marker       = 0;

        return true;
    }

    return false;
}

