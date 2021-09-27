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
