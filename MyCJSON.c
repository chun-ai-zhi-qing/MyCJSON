#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "MycJSON.h"

const char * exception;
const char * cJSON_GetErrorPtr(){return exception;};

static int cJSON_strcasecmp(const char *firstString,const char *secondString)
{
    if(!firstString)return (firstString == secondString)?0:1;//看看地址是不是一样的
    if(!secondString)return 1;
    for(;towlower(*firstString)==towlower(*secondString);secondString++,firstString++) if(*firstString==0)return 0;//如果一样返回去0
    return towlower(*firstString)-towlower(*secondString);
    //源代码：tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
    //我认为比较多余，故去掉
}

static void *(*cJSON_malloc)(size_t sz) = malloc;//定义一个函数指针并初始化指向malloc函数
static void (*cJSON_free)(void *ptr) = free;

static char * cJSON_String_Duplicate(const char * str)
{
    size_t len;
    len = strlen(str);
    char * dupstr;

    if(!(dupstr = (char * )cJSON_malloc(len + 1))) return NULL;
    memcpy(dupstr,str,len);
    return dupstr;
};

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if(!hooks){
        cJSON_malloc = malloc;
        cJSON_free = free;
    }
    cJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
    cJSON_free = (hooks->free_fn)?hooks->free_fn:free;
}

static cJSON * cJSON_NewItem(void)
{
    cJSON * node = (cJSON *)cJSON_malloc(sizeof(cJSON));
    /*if(!node)return NULL;
    return node;*///这里我写的但是看了原来的代码觉得其实原来的有点道理反正你是要返回，就全部返回去了
    if(node) memset(node,0,sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON * ptr)
{
    cJSON * next;
    while (ptr)
    {
        next = ptr->next;
        if(!(ptr->type&cJSON_IsReference)&&ptr->child) cJSON_Delete(ptr->child);
        if(!(ptr->type&cJSON_IsReference)&&ptr->valueString)cJSON_free(ptr->valueString);
        if(!(ptr->type&cJSON_StringIsConst)&&ptr->name) cJSON_free(ptr->name);
        cJSON_free(ptr);
        ptr = next;
    }
    
}

/* Parse the input text to generate a number, and populate the result into item. */
static const char *parse_number(cJSON * item,const char * num)
{
    double number=0,sign=1,scale=0;int subscale=0,signsubscale=1;
    //首先是符号，然后是前置0，之后数字，小数点，E
    //比如:-0.56421E5,-545.454E7
    if(*num == '-') {sign = -1;num++;}
    if(*num == '0')num++;//源代码是num++;	不会有非法数据0000.55456
    if(*num <= '9'&&*num>='0') {do{number = ((*num)-'0')+number * 10.0;num++;}while(*num>='9'&&*num>='0'); }
    if(*num == '.'&&num[1]>='0'&&num[1]<='9'){num++;do{scale--;number = ((*num - '0')+number*10.0);}while(*num<='9'&&*num>='0');}
    if(*num == 'e'|| *num == 'E') 
    {
        num++;
        if(*num=='+')num++;
        else if(*num=='-'){num++;signsubscale = -1;}
        while(*num>='0' && *num<='9'){subscale = subscale * 10 + (*num) - '0';num++;}
    }
    number = sign * number * pow(10.0,(scale + signsubscale * subscale));

    item -> valueDouble = number;
    item -> valueInt = (int) number;
    item -> type = cJSON_NUMBER;
    return num;
}
//最小二次方
static int pow2gt(int x){--x;	x|=x>>1;	x|=x>>2;	x|=x>>4;	x|=x>>8;	x|=x>>16;	return x+1;}

typedef struct {char *buffer; int length; int offset; } printbuffer;

static char *ensure(printbuffer *p,int needed)
{//确保enough
    char * newbuffer;
    int newsize;
    if(!p || !p -> buffer) return 0;
    needed += p->offset;
    if(needed<=p->length) return p->buffer + p->offset;

    newsize = pow2gt(needed);
    newbuffer = (char *)cJSON_malloc(newsize);
    if(!newbuffer){cJSON_free(p->buffer);p->length=0;p->buffer = 0;return 0;}
    if(newbuffer) memcpy(newbuffer,p->buffer,p->length);
    cJSON_free(p->buffer);
    p->length = newsize;
    p->buffer = newbuffer;
    return newbuffer + p -> offset;
}

static int update(printbuffer *p)
{
    char *str;
    if(!p || !p->buffer)return 0;
    str = p->buffer + p->offset;
    return p->offset + strlen(str);
}

static char * print_number(cJSON * item,printbuffer *p)
{
    char * str = 0;
    double d = item -> valueDouble;
    if(d == 0)
    {
        if(p) str = ensure(p,2);
        else str = (char *)cJSON_malloc(2);
        if(str) strcpy(str,"0");
    }
    else if(fabs(((double )item ->valueInt)-d)<=DBL_EPSILON&&d<=INT_MAX&&d>=INT_MIN)
    {
        if(p)   str = ensure(p,21);
        else    str = (char *)cJSON_malloc(21);
        if(str) sprintf(str,"%d",item->valueInt);
    }
    else
    {
        if(p) str =ensure(p,64);
        else  str = (char *)cJSON_malloc(64);
        if(str)
        {
            if(fabs(floor(d) - d)<=DBL_EPSILON && fabs(d)<1.0e60)   sprintf(str,"%.0f",d);
            else if(fabs(d)<1.0e-6||fabs(d)>1.0e9)                  sprintf(str,"%e",d);
            else                                                    sprintf(str,"%f",d);                       
        }
    }
    return str;
}

static unsigned parse_hex4(const char *str)//转换16进制的
{
    unsigned h=0;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	h=h<<4;str++;
	if (*str>='0' && *str<='9') h+=(*str)-'0'; else if (*str>='A' && *str<='F') h+=10+(*str)-'A'; else if (*str>='a' && *str<='f') h+=10+(*str)-'a'; else return 0;
	return h;

}
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(cJSON *item,const char *str)
{
    const char *ptr = str + 1;char *ptr2;char *out;int len = 0;unsigned uc,uc2;
    if(*str!='\"'){exception = str;return 0;}
    while(*ptr!='\"'&&*ptr&&++len)if(*ptr++ == '\\')ptr++;//出现"\\""
    out = (char *)cJSON_malloc(len + 1);
    if(!out) return 0;

    ptr = str + 1;
    ptr2 = out ;
    while(*ptr != '\"'&&*ptr)
    {
        if(*ptr!='\\') *ptr2++ = *ptr++;
        else
        {
            ptr++;
            switch (*ptr)
            {
                case 'b': * ptr2++ = '\b';break;
                case 'f': * ptr2++ = '\f';break;
                case 'n': * ptr2++ = '\n';break;
                case 'r': * ptr2++ = '\r';break;
                case 't': * ptr2++ = '\t';break;
                case 'u':
                    uc = parse_hex4(ptr + 1);
                    ptr+=4;

                    if((uc>=0xDC00&&uc<=0xDFFF)||uc==0)break;

                    if(uc>=0xD800&&uc<=0xDBFF)
                    {
                        if(ptr[1]!='\\'||ptr[2]!='u') break;
                        uc2 = parse_hex4(ptr + 3);
                        ptr+=6;
                        if(uc2<0xDC00||uc2>0xDFF)break;
                        uc = 0x10000 + (((uc & 0x3FF)<<10)|(uc2&0x3FF));
                    }
                    len = 4;
                    if(uc<0x80)len = 1;else if(uc<0x800) len = 2;else if(uc<0x10000)len = 3;
                    ptr2+=len;

                    switch (len)
                    {
                        case 4:*--ptr2 = ((uc|0x80)&0xBF);uc>>=6;
                        case 3:*--ptr2 = ((uc|0x80)&0xBF);uc>>=6;
                        case 2:*--ptr2 = ((uc|0x80)&0xBF);uc>>=6;
                        case 1:*--ptr2 = (uc|firstByteMark[len]);
                    }
                    ptr2+=len;
                    break;
                default:*ptr2++=*ptr;break;
                    
            }
            ptr++;
        }
        
    }
    *ptr2 = 0;
    if(*ptr =='\"') ptr++;
    item->valueString = out;
    item->type = cJSON_STRING;
    return ptr;
}


static char * print_string_ptr(const char * str,printbuffer *p)
{
    const char *ptr;
    char * ptr2,*out;
    int len = 0,flag = 0;
    unsigned char token;

    for(ptr = str;*ptr;ptr++)flag |= ((*ptr>0&&*ptr<32)||(*ptr == '\"')||(*ptr=='\\'))?1:0;
    
    if(!flag)
    {
        len = ptr -str;
        if(p) out = ensure(p,len+3);
        else out = (char *) cJSON_malloc(len + 3);
        if(!out) return 0;
        ptr2 = out;*ptr2++='\"';
        strcpy(ptr2,str);
        ptr2[len] = '\"';
        ptr2[len + 1] = 0;
        return out;
    }
    if(!str)
    {
        if(p)   out= ensure(p,3);
        else    out = (char *)cJSON_malloc(3);
        
        if(!out) return 0;
        strcpy(out,"\"\"");
        return out;
    }
    ptr = str;
    while((token = *ptr)&&++len){if (strchr("\"\\\b\f\n\r\t",token)) len++; else if (token<32) len+=5;ptr++;}

    if (p)	out=ensure(p,len+3);
	else	out=(char*)cJSON_malloc(len+3);
	if (!out) return 0;

    ptr2=out;ptr=str;
	*ptr2++='\"';
    while(*ptr)
    {
        if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (token=*ptr++)
			{
				case '\\':	*ptr2++='\\';	break;
				case '\"':	*ptr2++='\"';	break;
				case '\b':	*ptr2++='b';	break;
				case '\f':	*ptr2++='f';	break;
				case '\n':	*ptr2++='n';	break;
				case '\r':	*ptr2++='r';	break;
				case '\t':	*ptr2++='t';	break;
				default: sprintf(ptr2,"u%04x",token);ptr2+=5;	break;	/* escape and print */
			}
		}
    }
    *ptr2++='\"';*ptr2++=0;
	return out;
}
static char *print_string(cJSON *item,printbuffer *p)	{return print_string_ptr(item->valueString,p);}


static const char *parse_value(cJSON *item,const char *value);
static char *print_value(cJSON *item,int depth,int fmt,printbuffer *p);
static const char *parse_array(cJSON *item,const char *value);
static char *print_array(cJSON *item,int depth,int fmt,printbuffer *p);
static const char *parse_object(cJSON *item,const char *value);
static char *print_object(cJSON *item,int depth,int fmt,printbuffer *p);

static const char *skip(const char *in) {while (in && *in && (unsigned char)*in<=32) in++; return in;}

cJSON * cJSON_ParseWithOpts(const char * value,const char ** return_parse_end,int require_null_terminated)
{
    const char *end = 0;
    cJSON *c = cJSON_NewItem();
    exception = 0;
    if(!c) return 0;
    end = parse_value(c,skip(value));
    if(!end){cJSON_Delete(c);return 0;}

    if (require_null_terminated) {end=skip(end);if (*end) {cJSON_Delete(c);exception=end;return 0;}}
	if (return_parse_end) *return_parse_end=end;
	return c;
}
cJSON * cJSON_Parse(const char * value){return cJSON_ParseWithOpts(value,0,0);}

char *cJSON_Print(cJSON *item)				{return print_value(item,0,1,0);}
char *cJSON_PrintUnformatted(cJSON *item)	{return print_value(item,0,0,0);}
//未知
char * cJSON_PrintBufferrd(cJSON *item,int prebuffer,int fmt)
{
    printbuffer p;
    p.buffer = (char *)cJSON_malloc(prebuffer);
    p.length = prebuffer;
    p.offset = 0;
    
    return print_value(item,0,fmt,&p);
    return p.buffer;
}

static const char *parse_value(cJSON *item,const char *value)
{
    if(!value)                                          return 0;
    if(!strncmp(value,"null",4))                        {item->type = cJSON_NULL;return value+4;}
    if(!strncmp(value,"false",5))                       {item->type = cJSON_FALSE;return value+5;}
    if(!strncmp(value,"true",4))                        {item->type = cJSON_TRUE;return value+4;}
    if(*value == '\"')                                  {return parse_string(item,value);}
    if (*value=='-' || (*value>='0' && *value<='9'))    {return parse_number(item,value);}
    if(*value == '[')                                   {return parse_array(item,value);}
    if(*value == '{')                                   {return parse_object(item,value);}
    exception = value;
    return 0;/*failure*/
}

static char * print_value(cJSON *item,int depth,int fmt,printbuffer *p)
{
    char *out = 0;
    if(!item)   return 0;
    if(p)
    {
        switch ((item->type)&255)
        {
            case cJSON_NULL:    {out = ensure(p,5);     if(out) strcpy(out,"null");     break;}
            case cJSON_FALSE:   {out = ensure(p,6);     if(out) strcpy(out,"false");    break;}
            case cJSON_TRUE:    {out = ensure(p,5);     if(out) strcpy(out,"true");     break;}
            case cJSON_NUMBER:  out = print_number(item,p);break;
            case cJSON_STRING:  out = print_string(item,p);break;
            case cJSON_ARRAY:   out = print_array(item,depth,fmt,p); break;
            case cJSON_OBJECT:  out = print_object(item,depth,fmt,p);break;
        }
    }
    else
    {
        switch ((item->type)&255)
		{
			case cJSON_NULL:	out=cJSON_String_Duplicate("null");	break;
			case cJSON_FALSE:	out=cJSON_String_Duplicate("false");break;
			case cJSON_TRUE:	out=cJSON_String_Duplicate("true"); break;
			case cJSON_NUMBER:	out=print_number(item,0);break;
			case cJSON_STRING:	out=print_string(item,0);break;
			case cJSON_ARRAY:	out=print_array(item,depth,fmt,0);break;
			case cJSON_OBJECT:	out=print_object(item,depth,fmt,0);break;
		}
    }
    return out;
}

static char *print_array(cJSON * item,int depth,int fmt,printbuffer *p)
{
    char **entries;
    char *out = 0,*ptr,*ret;int len = 5;
    cJSON * child = item->child;
    int numentries = 0,fail=0,i=0;
    size_t tmplen=0;
    while(child){numentries++;child->next;}
    if(!numentries)
    {
        if(p) out = ensure(p,3);
        else    out = (char *)cJSON_malloc(3);
        if(out) strcpy(out,"[]");
        return out;
    }
    if(p)
    {
        i = p->offset;
        ptr = ensure(p,1);
        if(!ptr)return 0;
        *ptr = '[';
        p->offset++;
        child=item->child;
        while(child&&!fail)
        {
            print_value(child,depth+1,fmt,p);
            p->offset = update(p);
            if(child->next){len =fmt?2:1;ptr = ensure(p,len+1);if(!ptr)return 0;*ptr++=',';if(fmt)*ptr++=' ';*ptr = 0;p->offset+=len;}
            child = child ->next;
        }
        ptr=ensure(p,2);if (!ptr) return 0;	*ptr++=']';*ptr=0;
		out=(p->buffer)+i;
    }
    else
    {
        entries = (char**)cJSON_malloc(numentries * sizeof(char *));
        if(!entries)return 0;
        memset(entries,0,numentries * sizeof(char *));
        child = item ->child;
        while(child&&!fail)
        {
            ret = print_value(child,depth + 1,fmt , 0);
            entries[i++] = ret;
            if(ret) len += strlen(ret) + 2+(fmt?1:0);
            else fail = 1;
            child=child->next;
        }
        if (!fail)	out=(char*)cJSON_malloc(len);
        if(!out)    fail = 1;
        if(fail)
        {
            for (i=0;i<numentries;i++) if (entries[i]) cJSON_free(entries[i]);
			cJSON_free(entries);
			return 0;
        }

        *out = '[';
        ptr = out+1;
        *ptr = 0;
        for (i=0;i<numentries;i++)
		{
			tmplen=strlen(entries[i]);memcpy(ptr,entries[i],tmplen);ptr+=tmplen;
			if (i!=numentries-1) {*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;}
			cJSON_free(entries[i]);
		}
        cJSON_free(entries);
        *ptr++=']';
        *ptr++=0;

    }
    return out;
}

static const char *parse_object(cJSON *item,const char * value)
{
    cJSON * child;
    if(*value!='{'){exception = value;return 0;}

    item ->type = cJSON_OBJECT;
    value = skip(value + 1);
    if(*value=='{')return value + 1;

    item->child = child = cJSON_NewItem();
    if(!item->child) return value + 1;

    value = skip(parse_string(child,skip(value)));
    if(!value)  return 0;
    child->name = child->valueString;
    child->valueString  = 0;
    if(*value != ':'){exception = value;return 0;}
    value = skip(parse_value(child,skip(value+1)));
    if(!value)return 0;

    while(*value==',')
    {
        cJSON * new_item;
        if(!(new_item = cJSON_NewItem())) return 0;
        child ->next = new_item;
        new_item ->pre = child;
        child = new_item;
        value = skip(parse_string(child,skip(value+1)));
        if(!value)return 0;
        child->name = child->valueString;
        child->valueString = 0;
        if (*value!=':') {exception=value;return 0;}
        value = skip(parse_value(child,skip(value + 1)));
        if(!value)return 0;
    }

    if(*value == '}')return value+1;
    exception = value;
    return 0;
}

static char *print_object(cJSON *item,int depth,int fmt,printbuffer *p)
{
    char ** entries = 0,**names = 0;
    char *out = 0,*ptr,*ret,*str;
    int len = 7,i = 0,j;
    int numentries=0,fail=0;
    size_t tmplen = 0;
    cJSON *child=item->child;
    while(child)numentries++,child = child ->next;

    if(!numentries)
    {
        if(p)   out = ensure(p,fmt?depth+4:3);
        else    out = (char *)cJSON_malloc(fmt?depth+4:3);
        if(!out)return 0;
        ptr = out;
        *ptr ++ = '{';
        if(fmt){*ptr++='\n';for(i=0;i<depth-1;i++)*ptr++='\t';}
        *ptr++ = '}';
        *ptr++=0;
        return out;
    }
    if(p)
    {
        i = p->offset;
        len = fmt?2:1;
        ptr = ensure(p,len + 1);
        if(!ptr)    return 0;
        *ptr++ = '{';
        if(fmt) *ptr ++ ='\n';
        *ptr = 0;p->offset+=len;
        child = item->child;depth++;
        while(child)
        {
            if(fmt)
            {
                ptr = ensure(p,depth);
                if(!ptr)
                    return 0;
                for(j = 0;j<depth;j++)
                    *ptr ++ ='\t';
                p->offset+=depth;
            }
            print_string_ptr(child->name,p);
            p->offset = update(p);

            len =fmt?2:1;
            ptr = ensure(p,len);
            if(!ptr)return 0;
            *ptr++=':';
            if(fmt)*ptr++='\t';
            p->offset+=len;

            print_value(child,depth,fmt,p);
            p->offset=update(p);

            len =(fmt?1:0)+(child->next?1:0);
            ptr = ensure(p,len+1);
            if(!ptr)return 0;
            if(fmt) *ptr++ = '\n';
            *ptr = 0;
            p->offset += len;
            child = child->next;
        }
        ptr = ensure(p,fmt?(depth+1):2);
        if(!ptr) return 0;
        if(fmt) for(i = 0;i<depth - 1;i++) *ptr++ = '\t';
        *ptr++='{';
        *ptr=0;
        out = (p->buffer)+i;
    }
    else
    {
        entries = (char**)cJSON_malloc(numentries * sizeof(char * ));
        if(!entries)return 0;
        names = (char **)cJSON_malloc(numentries * sizeof(char *));
        if(!names){cJSON_free(entries);return 0;}
        memset(entries,0,sizeof(char *)*numentries);
        memset(names,0,sizeof(char *)*numentries);

        child = item->child;
        depth++;
        if(fmt) len+=depth;
        while(child)
        {
            names[i] = str = print_string_ptr(child->name,0);
            entries[i++] = ret =print_value(child,depth,fmt,0);
            if(str&&ret)len += strlen(ret) + strlen(str) + 2 + (fmt?2+depth:0);
            else fail = 1;
            child = child ->next;
        }

        if(!fail)   out = (char*)cJSON_malloc(len);
        if(!out)    fail = 1;

        if(fail)
        {
            for(i = 0;i<numentries;i++){if(names[i]) cJSON_free(names[i]);if(entries[i]) cJSON_free(entries[i]);}
            cJSON_free(names);
            cJSON_FALSE(entries);
            return 0;
        }
        *out = '{';
        ptr = out + 1;
        if(fmt) *ptr++='\n';
        *ptr = 0;
        for(i = 0;i<numentries;i++)
        {
            if(fmt) for(j = 0;j<depth;j++)  *ptr++='\t';
            tmplen = strlen(names[i]);
            memcpy(ptr,names[i],tmplen);
            ptr+=tmplen;
            *ptr++=':';if (fmt) *ptr++='\t';
			strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
			if (i!=numentries-1) *ptr++=',';
			if (fmt) *ptr++='\n';*ptr=0;
			cJSON_free(names[i]);
            cJSON_free(entries[i]);
        }
        cJSON_free(names);cJSON_free(entries);
		if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';
		*ptr++='}';*ptr++=0;
    }
    return out;
}

int cJSON_GetArraySize(cJSON *array)                            {cJSON * tmp = array->child;int i = 0;while(tmp)i++,tmp = tmp->next;return i;}
cJSON *cJSON_GetArrayItem(cJSON *array,int item)                {cJSON * tmp = array->child;while(tmp&&item>0)item--,tmp=tmp->next;return tmp;}
cJSON *cJSON_GetObjectItem(cJSON *object,const char *string)    {cJSON * tmp = object->child;while(tmp&&cJSON_strcasecmp(tmp->name,string))tmp = tmp->next;return tmp;}

static void suffix_object(cJSON *prev,cJSON * item){prev->next = item;item->pre = prev;}

static cJSON * create_reference(cJSON *item){cJSON *ref =cJSON_NewItem();if(!ref)return 0;memcpy(ref,item,sizeof(cJSON));ref->name =0;ref->type|=cJSON_IsReference;ref->next=ref->pre = 0;return ref;}

void cJSON_AddItemToArray(cJSON *array,cJSON *item)                             {cJSON * tmp = array->child;if(!item)return; if(!tmp){array->child = item;}else {while(tmp&&tmp->next)tmp = tmp->next;suffix_object(tmp,item);}}
void cJSON_AddItenToObject(cJSON *object,const char * string,cJSON *item)       {if(!item)return;if(item->name)cJSON_free(item->name);item->name = cJSON_String_Duplicate(string);cJSON_AddItemToArray(object,item);}
void cJSON_AddItemToObjectCS(cJSON *object,const char *string,cJSON *item)      {if (!item) return; if (!(item->type&cJSON_StringIsConst) && item->name) cJSON_free(item->name);item->name=(char*)string;item->type|=cJSON_StringIsConst;cJSON_AddItemToArray(object,item);}
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)				    {cJSON_AddItemToArray(array,create_reference(item));}
void cJSON_AddItemReferenceToObject(cJSON *object,const char *string,cJSON *item){cJSON_AddItemToObject(object,string,create_reference(item));}

cJSON * cJSON_DetachItemFromArray(cJSON *array,int which)
{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;
	if (c->pre) c->pre->next=c->next;if (c->next) c->next->pre=c->pre;if (c==array->child) array->child=c->next;c->pre=c->next=0;return c;}
void   cJSON_DeleteItemFromArray(cJSON *array,int which)			{cJSON_Delete(cJSON_DetachItemFromArray(array,which));}
cJSON * cJSON_DetachItemFromObject(cJSON * object,const char *string){int i = 0;cJSON * tmp = object->child;while(tmp&&cJSON_strcasecmp(tmp->name,string))i++,tmp = tmp->next;if(tmp)return cJSON_DetachItemFromArray(object,i);return 0;}
void    cJSON_DeleteItemFromObject(cJSON *object,const char *string){cJSON_Delete(cJSON_DetachItemFromObject(object,string));}


void   cJSON_InsertItemInArray(cJSON *array,int which,cJSON *newitem)		{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) {cJSON_AddItemToArray(array,newitem);return;}
	newitem->next=c;newitem->pre=c->pre;c->pre=newitem;if (c==array->child) array->child=newitem; else newitem->pre->next=newitem;}
void   cJSON_ReplaceItemInArray(cJSON *array,int which,cJSON *newitem)		{cJSON *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;
	newitem->next=c->next;newitem->pre=c->pre;if (newitem->next) newitem->next->pre=newitem;
	if (c==array->child) array->child=newitem; else newitem->pre->next=newitem;c->next=c->pre=0;cJSON_Delete(c);}
void   cJSON_ReplaceItemInObject(cJSON *object,const char *string,cJSON *newitem){int i=0;cJSON *c=object->child;while(c && cJSON_strcasecmp(c->name,string))i++,c=c->next;if(c){newitem->name=cJSON_String_Duplicate(string);cJSON_ReplaceItemInArray(object,i,newitem);}}

cJSON *cJSON_CreateNull(void)					{cJSON *item=cJSON_NewItem();if(item)item->type=cJSON_NULL;return item;}
cJSON *cJSON_CreateTrue(void)					{cJSON *item=cJSON_NewItem();if(item)item->type=cJSON_TRUE;return item;}
cJSON *cJSON_CreateFalse(void)					{cJSON *item=cJSON_NewItem();if(item)item->type=cJSON_FALSE;return item;}
cJSON *cJSON_CreateBool(int b)					{cJSON *item=cJSON_NewItem();if(item)item->type=b?cJSON_TRUE:cJSON_FALSE;return item;}
cJSON *cJSON_CreateNumber(double num)			{cJSON *item=cJSON_NewItem();if(item){item->type=cJSON_NUMBER;item->valueDouble=num;item->valueInt=(int)num;}return item;}
cJSON *cJSON_CreateString(const char *string)	{cJSON *item=cJSON_NewItem();if(item){item->type=cJSON_STRING;item->valueString=cJSON_String_Duplicate(string);}return item;}
cJSON *cJSON_CreateArray(void)					{cJSON *item=cJSON_NewItem();if(item)item->type=cJSON_ARRAY;return item;}
cJSON *cJSON_CreateObject(void)					{cJSON *item=cJSON_NewItem();if(item)item->type=cJSON_OBJECT;return item;}

cJSON *cJSON_CreateIntArray(const int *numbers,int count)		{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateFloatArray(const float *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateDoubleArray(const double *numbers,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
cJSON *cJSON_CreateStringArray(const char **strings,int count)	{int i;cJSON *n=0,*p=0,*a=cJSON_CreateArray();for(i=0;a && i<count;i++){n=cJSON_CreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}


cJSON *cJSON_Duplicate(cJSON *item,int recurse)
{
	cJSON *newitem,*cptr,*nptr=0,*newchild;
	/* Bail on bad ptr */
	if (!item) return 0;
	/* Create new item */
	newitem=cJSON_NewItem();
	if (!newitem) return 0;
	/* Copy over all vars */
	newitem->type=item->type&(~cJSON_IsReference),newitem->valueInt=item->valueInt,newitem->valueDouble=item->valueDouble;
	if (item->valueString)	{newitem->valueString=cJSON_String_Duplicate(item->valueString);	if (!newitem->valueString)	{cJSON_Delete(newitem);return 0;}}
	if (item->name)		{newitem->name=cJSON_String_Duplicate(item->name);			if (!newitem->name)		{cJSON_Delete(newitem);return 0;}}
	/* If non-recursive, then we're done! */
	if (!recurse) return newitem;
	/* Walk the ->next chain for the child. */
	cptr=item->child;
	while (cptr)
	{
		newchild=cJSON_Duplicate(cptr,1);		/* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {cJSON_Delete(newitem);return 0;}
		if (nptr)	{nptr->next=newchild,newchild->pre=nptr;nptr=newchild;}	/* If newitem->child already set, then crosswire ->prev and ->next and move on */
		else		{newitem->child=newchild;nptr=newchild;}					/* Set newitem->child and move to it */
		cptr=cptr->next;
	}
	return newitem;
}

void cJSON_Minify(char *json)
{
	char *into=json;
	while (*json)
	{
		if (*json==' ') json++;
		else if (*json=='\t') json++;	
		else if (*json=='\r') json++;
		else if (*json=='\n') json++;
		else if (*json=='/' && json[1]=='/')  while (*json && *json!='\n') json++;	
		else if (*json=='/' && json[1]=='*') {while (*json && !(*json=='*' && json[1]=='/')) json++;json+=2;}	
		else if (*json=='\"'){*into++=*json++;while (*json && *json!='\"'){if (*json=='\\') *into++=*json++;*into++=*json++;}*into++=*json++;} 
		else *into++=*json++;			
	}
	*into=0;	
}
