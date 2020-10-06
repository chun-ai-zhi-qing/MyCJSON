#ifndef _MycJSON_H_
#define _MycJSON_H_

//如果这个不是C++文件那么去掉以防错误
#ifdef __cplusplus
extern "C"
{
#endif

//cJSON Types.cJSON has seven types:number,false,true,null,object,array,string
#define cJSON_FALSE             0
#define cJSON_TRUE              1
#define cJSON_NULL              2
#define cJSON_NUMBER            3
#define cJSON_STRING            4
#define cJSON_ARRAY             5
#define cJSON_OBJECT            6

#define cJSON_IsReference       256
#define cJSON_StringIsConst     512

typedef struct mycJSON
{
    struct mycJSON      *pre,* next;                               //前后指针
    struct mycJSON      *child;                                  //数组、集合的时候需要申请空间
    
    int                 type;                                   //确定Types
    
    char *              valueString;                            //string这里赋值
    int                 valueInt;                               //true、false都是在这里赋值，数字赋值
    double              valueDouble;                            //数字在这里赋值

    char * name;                                                //名字
}cJSON;

typedef struct mycJSON_Hooks {
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} cJSON_Hooks;

//初始化Hook
extern void cJSON_InitHooks(cJSON_Hooks* hooks);

/*相互转换*/
//转换文本,转换cJSON
extern cJSON *      cJSON_Parse(const char * value);
//输出文本,转换文字                
extern char *       cJSON_Print(cJSON * item);
//unformately print to text                      
extern char *       cJSON_PrintUnformatted(cJSON * item);
//Render a cJSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted        
extern char *       cJSON_PrintBuffered(cJSON *item,int prebuffer,int fmt);
//delete cJSON entities
extern void         cJSON_Delete(cJSON *ptr);

//get size of array
extern int	        cJSON_GetArraySize(cJSON * array);
//Returns the element in the array marked index
extern cJSON *      cJSON_GetArrayItem(cJSON * array,int index);
//Get item "string" from object. Case insensitive.
extern cJSON *      cJSON_GetObjectItem(cJSON * object,const char * string);


/*error return a pointer to the parse error,success return null*/
extern const char *cJSON_GetErrorPtr(void);

/* These calls create a cJSON item of the appropriate type. */
extern cJSON *      cJSON_CreateNull(void);
extern cJSON *      cJSON_CreateTrue(void);
extern cJSON *      cJSON_CreateFalse(void);
extern cJSON *      cJSON_CreateBool(int boolean);
extern cJSON *      cJSON_CreateNumber(double number);
extern cJSON *      cJSON_CreateString(char * string);
extern cJSON *      cJSON_CreateArray(void);
extern cJSON *      cJSON_CreateObject(void);

/*These utilities create an Array of count items*/
extern cJSON *      cJSON_CreateIntArray(const int * numbers,int count);
extern cJSON *      cJSON_CreateFloatArray(const float * numbers,int count);
extern cJSON *      cJSON_CreateDoubleArray(const double * numbers,int count);
extern cJSON *      cJSON_CreateStringArray(const char ** strings,int count);
/*add item to array*/
extern void         cJSON_AddItemToArray(cJSON * array,cJSON * item);
extern void         cJSON_AddItemToObject(cJSON * array,const char *string,cJSON * object);
extern void	        cJSON_AddItemToObjectCS(cJSON *object,const char *string,cJSON *item);
extern void         cJSON_AddItemReferenceToArray(cJSON * array,cJSON * item);
extern void         cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item);
/*delete item from array*/
extern cJSON *      cJSON_DetachItemFromArray(cJSON * array,int which);
extern void         cJSON_DeleteItemFromArray(cJSON * array,int which);
extern cJSON *      cJSON_DetachItemFromArray(cJSON * array,const char * string);
extern void         cJSON_DeleteItemFromArray(cJSON * array,const char * string);
/*insert or repleace the item*/
extern void         cJSON_InsertItemInArray(cJSON * array,int which,cJSON *newItem);
extern void         cJSON_ReplaceItemInArray(cJSON * array,int which,cJSON * newItem);
extern void         cJSON_ReplaceItemInObject(cJSON *object,int which,cJSON * newItem);

extern cJSON *      cJSON_Duplicate(cJSON *item,int recurse);
extern cJSON *      cJSON_ParseWithOpts(const char *value,const char **return_parse_end,int require_null_terminated);
//clean somthing that not important such as /*,//
extern void cJSON_Minify(char *json);

#define cJSON_AddNullToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object,name)		cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddBoolToObject(object,name,b)	cJSON_AddItemToObject(object, name, cJSON_CreateBool(b))
#define cJSON_AddNumberToObject(object,name,n)	cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object,name,s)	cJSON_AddItemToObject(object, name, cJSON_CreateString(s))


#define cJSON_SetIntValue(object,val)			((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define cJSON_SetNumberValue(object,val)		((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif



#endif