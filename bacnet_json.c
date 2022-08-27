/*! JSON (JavaScript Object Notation) разбор текстового формата

    \see [ECMA-404] The JSON Data Interchange Standard. <http://www.json.org/json-ru.html>
    \see [RFC 7159] The JavaScript Object Notation (JSON) Data Interchange Format, March 2014
    <https://tools.ietf.org/html/rfc7159>
    \see [RFC 7396] JSON Merge Patch, October 2014
    \see [RFC 6901] JavaScript Object Notation (JSON) Pointer, April 2013
    \see [RFC 6902] JavaScript Object Notation (JSON) Patch, April 2013

Тестирование:
$ gcc -o test.exe json.c base64.c -lws2_32 `pkg-config --libs --cflags glib-2.0` -DTEST_JSON
 */
#include "bacnet.h"
#include "bacnet_asn.h"
#include "r3_slice.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>// определения isspace и isdigit

#define ASN_TYPE_ARRAY    0xF0


/*!
    Исходим из нескольких приближений
    1) Файл загружен целиком в динамическую память
    2) Можно резать файл? переписывать содержимое

    Бинарные данные произвольной длины положено передавать в кодировке base64Binary в форме строки
 */
/*! \brief освобождает память */
void bacnet_json_free(BACnetValue* js)
{

    /// . . .
}
typedef struct _PropertyDef PropertyDef_t;
struct _PropertyDef {
    const char* name;
    uint32_t id;
};


typedef struct _PropertyList PropertyList_t;
struct _PropertyList {
    union {
        uintptr_t property_id;
        char*    property_ref;
    };
    BACnetValue* value;
    struct _PropertyList* next;

};

const PropertyDef_t bacnet_property_ids[] = {
{"hello"}
};


/*! \brief выделяет тег при разборе формата strsep */
static char* bacnet_property_ref(char* s, char** tail)
{
    char* tag = 0;
    if(s[0]=='"'){
        s++;
        tag = s;
        while (s[0]!='"' && s[0]!='\0')s++;
        if (s[0]=='"') {
            *s++ ='\0';
            // PropertyDef_t * ref = bsearch(tag, bacnet_property_ids, sizeof(bacnet_property_ids)/sizeof(struct _PropertyDef), sizeof(struct _PropertyDef), (int (*)(const void*, const void*))strcmp);
            //if (ref) id = ref->id;
            //*s++//;='"';
        }
    }
    if (tail)*tail=s;
    return tag;
}
//! \todo копировать строку!
static char* json_string_value(char* s, char** tail)
{
    char* value = s;
    while (s[0]!='"'  && s[0]!='\0'){
        if(s[0]=='\\' && s[1]=='"') s+=2;
        else s++;
    }
    if(s[0]=='"') {
        s++;//*s++='\0';
    }
    *tail = s;
    return value;
}
static inline BACnetValue * bacnet_json_new(uint32_t tag)
{
    BACnetValue *js = g_slice_alloc(sizeof(BACnetValue));
    bacnet_value_init(js, tag);
    return js;
}
/*! \brief выполнить разбор строки данных */
BACnetValue *bacnet_json_value(char* s, char** tail)
{
    BACnetValue * js = NULL;
    while (isspace(s[0]))s++;
    if (s[0]=='\0') goto do_exit;
    switch(*s++){
    case '"': {// начало строки или поля объекта
            //js = json_new(JSON_STRING); // созданная строка
            js = g_slice_alloc(sizeof(BACnetValue));
            js->tag = ASN_TYPE_STRING;
            js->type_id = 0;
            char* str = json_string_value(s, &s);
            js->value.s = str;//(s>str)?g_strndup(str, s - str-1):NULL;
            js->length  = s-str;
        }
        break;
    case '[': {// массив
        js = g_slice_alloc(sizeof(BACnetValue));
        js->tag = ASN_CLASS_CONSTRUCTIVE|1;// массив!
        js->type_id = 0;
//        js = json_new(JSON_ARRAY);
        BACnetLIST* list =NULL;
        while (isspace(s[0]))s++;
        while (s[0]!=']' && s[0]!='\0') {
            BACnetValue* js_elem = bacnet_json_value(s, &s);
            if (js_elem) {
                BACnetLIST * next = g_slice_alloc(sizeof(BACnetLIST));
                if (list==NULL){
                    js->value.list = next;
                } else
                    list->next = next;
                list = next;
                list->next = NULL;
            }
            if (s[0]==',') s++;
            else
                break;
            while (isspace(s[0]))s++;
        }
    } break;
    case '{': {// объект
        //js = json_new(JSON_OBJECT);
        js = bacnet_json_new(ASN_CLASS_CONSTRUCTIVE);
        PropertyList_t* list = NULL;
        while(isspace(s[0])) s++;
        while (s[0]!='}' && s[0]!='\0') {
            char* prop_ref = bacnet_property_ref(s, &s); // без копирования строки, можно временно вставить символ конца строки
            while(isspace(s[0])) s++;
            if(s[0]==':') {
                s++;
                BACnetValue* js_elem = bacnet_json_value(s, &s);
                if (js_elem){
                    PropertyList_t * next = g_slice_alloc(sizeof(PropertyList_t));
                    if (list==NULL){
                        js->value.ptr = next;
                    } else
                        list->next = next;
                    list = next;
                    list->value = js_elem;
                    list->property_ref = prop_ref;
                    list->next = NULL;
//                    list = property_list_append(list, prop_id, js_elem);
                }
            } else {
                //error = g_error_new();
                break;
            }
            if (s[0]==',') s++;
            else
                break;
            while(isspace(s[0])) s++;
        }
        if(s[0]=='}')s++;
        //js->value.list = list;
        // else err=;
    } break;
    case 't': {// true
        if (strncmp(s, "rue", 3)==0){
            js = bacnet_json_new(ASN_TYPE_BOOLEAN|1);
            js->value.u = 1;
            s+=3;
        } //else err=;
    } break;
    case 'f': {// false
        if (strncmp(s, "alse", 4)==0){
            js = bacnet_json_new(ASN_TYPE_BOOLEAN);
            js->value.u = 0;
            s+=4;
        }// else err=;
    } break;
    case 'n': {// null
        if (strncmp(s, "ull", 3)==0){
            js = bacnet_json_new(ASN_TYPE_NULL);
            s+=3;
        }// else err=;

    } break;
    case '-':// число со знаком
    case '0'...'9': {
            s--;
            char* ref = s;
            if (s[0]=='0' && s[1]=='x'){// не является частью стандарта, шестнадцатеричные числа
                ref+=2;
                js = bacnet_json_new(ASN_TYPE_UNSIGNED|4);
                js->value.i = strtol(ref, &s, 16);
                break;
            }
            if(s[0]=='-')s++;
            while (isdigit(s[0])) s++;
            if(s[0]=='.'){// вещественное число
                js = bacnet_json_new(ASN_TYPE_REAL|4);
                js->value.f = strtof(ref, &s);
            } else {// целое число
                js = bacnet_json_new(ASN_TYPE_INTEGER);
                js->value.i = strtol(ref, &s, 10);
            }
    } break;
    default:
        break;
    }
    while (isspace(s[0]))s++;
do_exit:
    if(tail)*tail = s;
    return js;
}
#if 0
/*! \brief */
void bacnet_json_to_string(BACnetValue* js, GString* str, int offset)
{
    if (js==NULL) { // проверить на практике, может быть плохая идея
        g_string_append(str, "null");
        return;
    }
    if (js->tag_id) g_string_append_printf(str, "%*s\"%s\":", offset, "",g_quark_to_string(js->tag_id));
    switch (js->type){
    case JSON_ARRAY: {
        GSList* list = js->value.list;
        g_string_append(str, "[\n");
        while (list) {
            json_to_string((JsonNode*)list->data, str, offset+2);
            g_string_append(str, ",\n");
            list = list->next;
        }
        g_string_append(str, "]");
    } break;
    case JSON_OBJECT: {
        GSList* list = js->value.list;
        g_string_append(str, "{\n");
        while (list) {
            json_to_string((JsonNode*)list->data, str, offset+2);
            g_string_append(str, ",\n");
            list = list->next;
        }
        g_string_append_printf(str, "%*s}", offset, "");
    } break;
    case JSON_NULL:
        g_string_append(str, "null");
        break;
    case JSON_BOOL:
        g_string_append(str, js->value.b!=0?"true":"false");
        break;
    case JSON_INT:
        g_string_append_printf(str, "%lld", js->value.i);
        break;
    case JSON_DOUBLE:
        g_string_append_printf(str, "%g", js->value.f);
        break;
    case JSON_STRING:
        g_string_append_printf(str, "\"%s\"", js->value.s);
        break;
    default:
        g_print("ERR:undefined type\n");
        break;
    }
}
#endif // 0
/*! \brief получить элемент списка - по идентификатору */
BACnetValue* bacnet_json_object_get(BACnetValue* js, uint32_t property_id)
{
    if (js==NULL || js->tag!=(ASN_CLASS_CONSTRUCTIVE)) return NULL;
    PropertyList_t* list = js->value.ptr;
    while (list){
        if(list->property_id == property_id){
            return list->value;
        }
        list = list->next;
    }
    return NULL;
}

/*! \brief выборка элемента структуры данных по пути
    \param id - список идентификаторов, заканчивается нулем

    \todo к элементам массива можно обратиться по номеру элемента

*/
#if 0
JsonNode* json_object_path(JsonNode* js, GQuark* id)
{
    while (*id!=0 && js) {
        if (js->type==JSON_OBJECT) {
            GSList* list = json_object_get_(js->value.list, *id++);
            js = list!=NULL? list->data: NULL;
        } else
        if (js->type==JSON_ARRAY) {
            GSList* list = g_slist_nth(js->value.list, *id++);
            js = list!=NULL? list->data: NULL;
        } else {
            return NULL;
        }
    }
    return js;
}
#endif // 0
/*! \brief удалить элемент из списка свойств объекта
    \return значение из сипска
*/
BACnetValue* bacnet_json_object_remove(BACnetValue* js, uint32_t property_id)
{
    if (js==NULL || js->tag!=(ASN_CLASS_CONSTRUCTIVE)) return NULL;

    PropertyList_t* list = js->value.ptr;
    PropertyList_t* prev = NULL;
    while (list){
        if (list->property_id==property_id) {
            BACnetValue* value = list->value;
            if (prev) { // стоит не первым в списке
                prev->next = list->next;
            } else { // в списке первый
                js->value.ptr = list->next;
            }
            g_slice_free1(sizeof(PropertyList_t), list);
            return value;
        }
        prev = list;
        list = list->next;
    }

    return NULL;
}

/*!
 define MergePatch(Target, Patch):
     if Patch is an Object:
       if Target is not an Object:
         Target = {} # Ignore the contents and set it to an empty Object
       for each Name/Value pair in Patch:
         if Value is null:
           if Name exists in Target:
             remove the Name/Value pair from Target
         else:
           Target[Name] = MergePatch(Target[Name], Value)
       return Target
     else:
       return Patch

   PATCH /my/resource HTTP/1.1
   Host: example.org
   Content-Type: application/merge-patch+json

   {
     "title": "Hello!",
     "phoneNumber": "+01-123-456-7890",
     "author": {
       "familyName": null
     },
     "tags": [ "example" ]
   }

 */

#ifdef TEST_JSON_PATCH
char t1[] = "{\"foo\": \"bar\"}" ;
char p1[] = "{ \"op\": \"add\", \"path\": \"/baz\", \"value\": \"qux\" }";
char t2[] = "{ \"foo\": [ \"bar\", \"baz\" ] }";
char p2[] = "{ \"op\": \"add\", \"path\": \"/foo/1\", \"value\": \"qux\" }";
char t3[] = "{\"baz\": \"qux\",\"foo\": \"bar\"}";
char p3[] = "{ \"op\": \"remove\", \"path\": \"/baz\" }";
char t4[] = "{ \"foo\": [ \"bar\", \"qux\", \"baz\" ] }";
char p4[] = "{ \"op\": \"remove\", \"path\": \"/foo/1\" }";
char t5[] = "{ \"baz\": \"qux\", \"foo\": \"bar\" }";
char p5[] = "{ \"op\": \"replace\", \"path\": \"/baz\", \"value\": \"boo\" }";
char t6[] = "{ \"foo\": { \"bar\": \"baz\", \"waldo\": \"fred\"  },  \"qux\": { \"corge\": \"grault\"   } }";
char p6[] = "{ \"op\": \"move\", \"from\": \"/foo/waldo\", \"path\": \"/qux/thud\" }";
// A.10.  Adding a Nested Member Object
char t10[] = "{ \"foo\": \"bar\" }";
char p10[] = "{ \"op\": \"add\", \"path\": \"/child\", \"value\": { \"grandchild\": { } } }";
// A.11.  Ignoring Unrecognized Elements
char t11[] = "{ \"foo\": \"bar\" }";
char p11[] = "";
// A.12.  Adding to a Nonexistent Target
char t12[] = "{ \"foo\": \"bar\" }";
char p12[] = "{ \"op\": \"add\", \"path\": \"/baz/bat\", \"value\": \"qux\" }";
// A.16.  Adding an Array Value
char t16[] = "{ \"foo\": [\"bar\"] }";
char p16[] = "{ \"op\": \"add\", \"path\": \"/foo/-\", \"value\": [\"abc\", \"def\"] }";
int main(int argc, char**argv)
{
//    if (argc<3) return 0;
//    g_print("> %s\n", t1);
//    g_print("> %s\n", p1);
    JsonNode* js = json_value(t12, NULL, NULL);
    JsonNode* patch = json_value(p12, NULL, NULL);
    GString* str = g_string_new(NULL);
    json_to_string(js, str, 0);
    json_to_string(patch, str, 0);
    g_print("%s\n", str->str);
    int res = json_patch(js, patch);
    g_string_truncate(str,0);
    json_to_string(js, str, 0);
    g_print("%s\n", str->str);
    json_free(js);
    json_free(patch);
    return res;//json_patch(js, patch);
}
#endif // TEST_JSON_PATCH

#ifdef TEST_JSON
int main()
{
char* str1 =
"{\"access_token\":\"1DE731E8\",\n"
"\"token_type\":\"Bearer\",\n"
"\"expires_in\":3600,\n"
"\"refresh_token\":\"0EF398F4\",\n}";


char* str2 =
"{\".definitions\": {\n"
    "\"BACnetFileAccessMethod\":{"
        "\"$base\": \"Enumerated\","
        "\"$namedValues\": {"
            "\"recordAccess\":{ \"$base\":\"Unsigned\", \"value\":0 },"
            "\"streamAccess\":{ \"$base\":\"Unsigned\", \"value\":1 }"
        "}"
    "}"
"}";
    str1 = g_strdup(str2);
    JsonNode* js= json_value(str1, NULL, NULL);
    GString* str = g_string_sized_new(127);
    json_to_string(js, str, 0);
    g_print("%s\n", str->str);


    JsonNode* elem = json_object_get(js, g_quark_from_string("token_type"));
    if(elem) g_print("found value=%s\n", elem->value.s);
    json_free(js);

    return 0;
}


#endif // TEST_JSON
