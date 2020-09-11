/*
 * XML DRI client-side driver configuration
 * Copyright (C) 2003 Felix Kuehling
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * FELIX KUEHLING, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */
/**
 * \file xmlconfig.c
 * \brief Driver-independent client-side part of the XML configuration
 * \author Felix Kuehling
 */

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if XMLCONFIG
#include <expat.h>
#endif
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>
#include <regex.h>
#include "strndup.h"
#include "xmlconfig.h"
#include "u_process.h"
#include "os_file.h"

/* For systems like Hurd */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if XMLCONFIG
static bool
be_verbose(void)
{
   const char *s = getenv("MESA_DEBUG");
   if (!s)
      return true;

   return strstr(s, "silent") == NULL;
}
#endif

static driOptionInfo *
lookupInfo(const driOptionCache *cache, const char *name)
{
   struct hash_entry *entry = _mesa_hash_table_search(cache->info, name);
   if (entry)
      return entry->data;
   return NULL;
}

static driOptionValue *
lookupValue(const driOptionCache *cache, const char *name)
{
   struct hash_entry *entry = _mesa_hash_table_search(cache->values, name);

   /* We store the values in the pointer field of the hash tables. */
   STATIC_ASSERT(sizeof(entry->data) >= sizeof(driOptionValue));

   if (!entry)
      entry = _mesa_hash_table_insert(cache->values, name, 0);
   return (driOptionValue *)(void *)(&entry->data);
}


#define ALLOC_CHECK(x) do {                                             \
      if (!(x)) {                                                       \
         fprintf (stderr, "%s: %d: out of memory.\n", __FILE__, __LINE__); \
         abort();                                                       \
      }                                                                 \
   } while (0)

/** \brief Like strdup with error checking. */
#define XSTRDUP(dest,source) do {                                       \
      dest = strdup(source);                                            \
      ALLOC_CHECK(dest);                                                \
   } while (0)

#if XMLCONFIG
static int compare (const void *a, const void *b) {
   return strcmp (*(char *const*)a, *(char *const*)b);
}
/** \brief Binary search in a string array. */
static uint32_t
bsearchStr (const char *name, const char *elems[], uint32_t count)
{
   const char **found;
   found = bsearch (&name, elems, count, sizeof (char *), compare);
   if (found)
      return found - elems;
   else
      return count;
}
#endif

/** \brief Locale-independent integer parser.
 *
 * Works similar to strtol. Leading space is NOT skipped. The input
 * number may have an optional sign. Radix is specified by base. If
 * base is 0 then decimal is assumed unless the input number is
 * prefixed by 0x or 0X for hexadecimal or 0 for octal. After
 * returning tail points to the first character that is not part of
 * the integer number. If no number was found then tail points to the
 * start of the input string. */
static int
strToI(const char *string, const char **tail, int base)
{
   int radix = base == 0 ? 10 : base;
   int result = 0;
   int sign = 1;
   bool numberFound = false;
   const char *start = string;

   assert (radix >= 2 && radix <= 36);

   if (*string == '-') {
      sign = -1;
      string++;
   } else if (*string == '+')
      string++;
   if (base == 0 && *string == '0') {
      numberFound = true;
      if (*(string+1) == 'x' || *(string+1) == 'X') {
         radix = 16;
         string += 2;
      } else {
         radix = 8;
         string++;
      }
   }
   do {
      int digit = -1;
      if (radix <= 10) {
         if (*string >= '0' && *string < '0' + radix)
            digit = *string - '0';
      } else {
         if (*string >= '0' && *string <= '9')
            digit = *string - '0';
         else if (*string >= 'a' && *string < 'a' + radix - 10)
            digit = *string - 'a' + 10;
         else if (*string >= 'A' && *string < 'A' + radix - 10)
            digit = *string - 'A' + 10;
      }
      if (digit != -1) {
         numberFound = true;
         result = radix*result + digit;
         string++;
      } else
         break;
   } while (true);
   *tail = numberFound ? string : start;
   return sign * result;
}

/** \brief Locale-independent floating-point parser.
 *
 * Works similar to strtod. Leading space is NOT skipped. The input
 * number may have an optional sign. '.' is interpreted as decimal
 * point and may occur at most once. Optionally the number may end in
 * [eE]<exponent>, where <exponent> is an integer as recognized by
 * strToI. In that case the result is number * 10^exponent. After
 * returning tail points to the first character that is not part of
 * the floating point number. If no number was found then tail points
 * to the start of the input string.
 *
 * Uses two passes for maximum accuracy. */
static float
strToF(const char *string, const char **tail)
{
   int nDigits = 0, pointPos, exponent;
   float sign = 1.0f, result = 0.0f, scale;
   const char *start = string, *numStart;

   /* sign */
   if (*string == '-') {
      sign = -1.0f;
      string++;
   } else if (*string == '+')
      string++;

   /* first pass: determine position of decimal point, number of
    * digits, exponent and the end of the number. */
   numStart = string;
   while (*string >= '0' && *string <= '9') {
      string++;
      nDigits++;
   }
   pointPos = nDigits;
   if (*string == '.') {
      string++;
      while (*string >= '0' && *string <= '9') {
         string++;
         nDigits++;
      }
   }
   if (nDigits == 0) {
      /* no digits, no number */
      *tail = start;
      return 0.0f;
   }
   *tail = string;
   if (*string == 'e' || *string == 'E') {
      const char *expTail;
      exponent = strToI (string+1, &expTail, 10);
      if (expTail == string+1)
         exponent = 0;
      else
         *tail = expTail;
   } else
      exponent = 0;
   string = numStart;

   /* scale of the first digit */
   scale = sign * (float)pow (10.0, (double)(pointPos-1 + exponent));

   /* second pass: parse digits */
   do {
      if (*string != '.') {
         assert (*string >= '0' && *string <= '9');
         result += scale * (float)(*string - '0');
         scale *= 0.1f;
         nDigits--;
      }
      string++;
   } while (nDigits > 0);

   return result;
}

/** \brief Parse a value of a given type. */
static unsigned char
parseValue(driOptionValue *v, driOptionType type, const char *string)
{
   const char *tail = NULL;
   /* skip leading white-space */
   string += strspn (string, " \f\n\r\t\v");
   switch (type) {
   case DRI_BOOL:
      if (!strcmp (string, "false")) {
         v->_bool = false;
         tail = string + 5;
      } else if (!strcmp (string, "true")) {
         v->_bool = true;
         tail = string + 4;
      }
      else
         return false;
      break;
   case DRI_ENUM: /* enum is just a special integer */
   case DRI_INT:
      v->_int = strToI (string, &tail, 0);
      break;
   case DRI_FLOAT:
      v->_float = strToF (string, &tail);
      break;
   case DRI_STRING:
      ralloc_free (v->_string);
      v->_string = strndup(string, STRING_CONF_MAXLEN);
      return true;
   }

   if (tail == string)
      return false; /* empty string (or containing only white-space) */
   /* skip trailing white space */
   if (*tail)
      tail += strspn (tail, " \f\n\r\t\v");
   if (*tail)
      return false; /* something left over that is not part of value */

   return true;
}

static bool
lookupType(const char *str, driOptionType *type)
{
   if (!strcmp (str, "bool"))
      *type = DRI_BOOL;
   else if (!strcmp (str, "enum"))
      *type = DRI_ENUM;
   else if (!strcmp (str, "int"))
      *type = DRI_INT;
   else if (!strcmp (str, "float"))
      *type = DRI_FLOAT;
   else if (!strcmp (str, "string"))
      *type = DRI_STRING;
   else
      return false;

   return true;
}

#if XMLCONFIG
/** \brief Parse a list of ranges of type info->type. */
static unsigned char
parseRanges(driOptionInfo *info, const char *string)
{
   char *cp, *range;
   uint32_t nRanges, i;
   driOptionRange *ranges;

   XSTRDUP (cp, string);
   /* pass 1: determine the number of ranges (number of commas + 1) */
   range = cp;
   for (nRanges = 1; *range; ++range)
      if (*range == ',')
         ++nRanges;

   ranges = ralloc_array(info, driOptionRange, nRanges);
   ALLOC_CHECK(ranges);

   /* pass 2: parse all ranges into preallocated array */
   range = cp;
   for (i = 0; i < nRanges; ++i) {
      char *end, *sep;
      assert (range);
      end = strchr (range, ',');
      if (end)
         *end = '\0';
      sep = strchr (range, ':');
      if (sep) { /* non-empty interval */
         *sep = '\0';
         if (!parseValue (&ranges[i].start, info->type, range) ||
             !parseValue (&ranges[i].end, info->type, sep+1))
            break;
         if (info->type == DRI_INT &&
             ranges[i].start._int > ranges[i].end._int)
            break;
         if (info->type == DRI_FLOAT &&
             ranges[i].start._float > ranges[i].end._float)
            break;
      } else { /* empty interval */
         if (!parseValue (&ranges[i].start, info->type, range))
            break;
         ranges[i].end = ranges[i].start;
      }
      if (end)
         range = end+1;
      else
         range = NULL;
   }
   free(cp);
   if (i < nRanges) {
      ralloc_free(ranges);
      return false;
   } else
      assert (range == NULL);

   info->nRanges = nRanges;
   info->ranges = ranges;
   return true;
}

/** \brief Check if a value is in one of info->ranges. */
static bool
checkValue(const driOptionValue *v, const driOptionInfo *info)
{
   uint32_t i;
   assert (info->type != DRI_BOOL); /* should be caught by the parser */
   if (info->nRanges == 0)
      return true;
   switch (info->type) {
   case DRI_ENUM: /* enum is just a special integer */
   case DRI_INT:
      for (i = 0; i < info->nRanges; ++i)
         if (v->_int >= info->ranges[i].start._int &&
             v->_int <= info->ranges[i].end._int)
            return true;
      break;
   case DRI_FLOAT:
      for (i = 0; i < info->nRanges; ++i)
         if (v->_float >= info->ranges[i].start._float &&
             v->_float <= info->ranges[i].end._float)
            return true;
      break;
   case DRI_STRING:
      break;
   default:
      assert (0); /* should never happen */
   }
   return false;
}

/**
 * Print message to \c stderr if the \c LIBGL_DEBUG environment variable
 * is set. 
 * 
 * Is called from the drivers.
 * 
 * \param f \c printf like format string.
 */
static void
__driUtilMessage(const char *f, ...)
{
   va_list args;
   const char *libgl_debug;

   libgl_debug=getenv("LIBGL_DEBUG");
   if (libgl_debug && !strstr(libgl_debug, "quiet")) {
      fprintf(stderr, "libGL: ");
      va_start(args, f);
      vfprintf(stderr, f, args);
      va_end(args);
      fprintf(stderr, "\n");
   }
}

/** \brief Output a warning message. */
#define XML_WARNING1(msg) do {                                          \
      __driUtilMessage ("Warning in %s line %d, column %d: "msg, data->name, \
                        (int) XML_GetCurrentLineNumber(data->parser),   \
                        (int) XML_GetCurrentColumnNumber(data->parser)); \
   } while (0)
#define XML_WARNING(msg, ...) do {                                      \
      __driUtilMessage ("Warning in %s line %d, column %d: "msg, data->name, \
                        (int) XML_GetCurrentLineNumber(data->parser),   \
                        (int) XML_GetCurrentColumnNumber(data->parser), \
                        ##__VA_ARGS__);                                 \
   } while (0)
/** \brief Output an error message. */
#define XML_ERROR1(msg) do {                                            \
      __driUtilMessage ("Error in %s line %d, column %d: "msg, data->name, \
                        (int) XML_GetCurrentLineNumber(data->parser),   \
                        (int) XML_GetCurrentColumnNumber(data->parser)); \
   } while (0)
#define XML_ERROR(msg, ...) do {                                        \
      __driUtilMessage ("Error in %s line %d, column %d: "msg, data->name, \
                        (int) XML_GetCurrentLineNumber(data->parser),   \
                        (int) XML_GetCurrentColumnNumber(data->parser), \
                        ##__VA_ARGS__);                                 \
   } while (0)
/** \brief Output a fatal error message and abort. */
#define XML_FATAL1(msg) do {                                            \
      fprintf (stderr, "Fatal error in %s line %d, column %d: "msg"\n", \
               data->name,                                              \
               (int) XML_GetCurrentLineNumber(data->parser),            \
               (int) XML_GetCurrentColumnNumber(data->parser));         \
      abort();                                                          \
   } while (0)
#define XML_FATAL(msg, ...) do {                                        \
      fprintf (stderr, "Fatal error in %s line %d, column %d: "msg"\n", \
               data->name,                                              \
               (int) XML_GetCurrentLineNumber(data->parser),            \
               (int) XML_GetCurrentColumnNumber(data->parser),          \
               ##__VA_ARGS__);                                          \
      abort();                                                          \
   } while (0)

/** \brief Parser context for __driConfigOptions. */
struct OptInfoData {
   const char *name;
   XML_Parser parser;
   driOptionCache *cache;
   bool inDriInfo;
   bool inSection;
   bool inDesc;
   bool inOption;
   bool inEnum;
   driOptionInfo *curOption;
};

/** \brief Elements in __driConfigOptions. */
enum OptInfoElem {
   OI_DESCRIPTION = 0, OI_DRIINFO, OI_ENUM, OI_OPTION, OI_SECTION, OI_COUNT
};
static const char *OptInfoElems[] = {
   "description", "driinfo", "enum", "option", "section"
};

/** \brief Parse attributes of an enum element.
 *
 * We're not actually interested in the data. Just make sure this is ok
 * for external configuration tools.
 */
static void
parseEnumAttr(struct OptInfoData *data, const char **attr)
{
   uint32_t i;
   const char *value = NULL, *text = NULL;
   driOptionValue v;
   for (i = 0; attr[i]; i += 2) {
      if (!strcmp (attr[i], "value")) value = attr[i+1];
      else if (!strcmp (attr[i], "text")) text = attr[i+1];
      else XML_FATAL("illegal enum attribute: %s.", attr[i]);
   }
   if (!value) XML_FATAL1 ("value attribute missing in enum.");
   if (!text) XML_FATAL1 ("text attribute missing in enum.");
   if (!parseValue (&v, data->curOption->type, value))
      XML_FATAL ("illegal enum value: %s.", value);
   if (!checkValue (&v, data->curOption))
      XML_FATAL ("enum value out of valid range: %s.", value);
}

/** \brief Parse attributes of a description element.
 *
 * We're not actually interested in the data. Just make sure this is ok
 * for external configuration tools.
 */
static void
parseDescAttr(struct OptInfoData *data, const char **attr)
{
   uint32_t i;
   const char *lang = NULL, *text = NULL;
   for (i = 0; attr[i]; i += 2) {
      if (!strcmp (attr[i], "lang")) lang = attr[i+1];
      else if (!strcmp (attr[i], "text")) text = attr[i+1];
      else XML_FATAL("illegal description attribute: %s.", attr[i]);
   }
   if (!lang) XML_FATAL1 ("lang attribute missing in description.");
   if (!text) XML_FATAL1 ("text attribute missing in description.");
}

/** \brief Parse attributes of an option element. */
static void
parseOptInfoAttr(struct OptInfoData *data, const char **attr)
{
   enum OptAttr {OA_DEFAULT = 0, OA_NAME, OA_TYPE, OA_VALID, OA_COUNT};
   static const char *optAttr[] = {"default", "name", "type", "valid"};
   const char *attrVal[OA_COUNT] = {NULL, NULL, NULL, NULL};
   const char *defaultVal;
   driOptionCache *cache = data->cache;
   driOptionInfo *opt;
   uint32_t i;
   for (i = 0; attr[i]; i += 2) {
      uint32_t attrName = bsearchStr (attr[i], optAttr, OA_COUNT);
      if (attrName >= OA_COUNT)
         XML_FATAL ("illegal option attribute: %s", attr[i]);
      attrVal[attrName] = attr[i+1];
   }
   if (!attrVal[OA_NAME]) XML_FATAL1 ("name attribute missing in option.");
   if (!attrVal[OA_TYPE]) XML_FATAL1 ("type attribute missing in option.");
   if (!attrVal[OA_DEFAULT]) XML_FATAL1 ("default attribute missing in option.");

   opt = ralloc(cache->info, driOptionInfo);
   ALLOC_CHECK(opt);

   opt->name = ralloc_strdup(opt, attrVal[OA_NAME]);
   ALLOC_CHECK(opt->name);
   data->curOption = opt;

   if (lookupInfo(cache, opt->name))
      XML_FATAL ("option %s redefined.", attrVal[OA_NAME]);

   _mesa_hash_table_insert(cache->info, opt->name, opt);

   if (!lookupType(attrVal[OA_TYPE], &opt->type))
      XML_FATAL ("illegal type in option: %s.", attrVal[OA_TYPE]);

   defaultVal = getenv (opt->name);
   if (defaultVal != NULL) {
      /* don't use XML_WARNING, we want the user to see this! */
      if (be_verbose()) {
         fprintf(stderr,
                 "ATTENTION: default value of option %s overridden by environment.\n",
                 opt->name);
      }
   } else
      defaultVal = attrVal[OA_DEFAULT];
   driOptionValue *val = lookupValue(cache, opt->name);
   if (!parseValue (val, opt->type, defaultVal))
      XML_FATAL ("illegal default value for %s: %s.", opt->name, defaultVal);

   if (attrVal[OA_VALID]) {
      if (opt->type == DRI_BOOL)
         XML_FATAL1 ("boolean option with valid attribute.");
      if (!parseRanges (opt, attrVal[OA_VALID]))
         XML_FATAL ("illegal valid attribute: %s.", attrVal[OA_VALID]);
      if (!checkValue (val, opt))
         XML_FATAL ("default value out of valid range '%s': %s.",
                    attrVal[OA_VALID], defaultVal);
   } else if (opt->type == DRI_ENUM) {
      XML_FATAL1 ("valid attribute missing in option (mandatory for enums).");
   } else {
      opt->nRanges = 0;
      opt->ranges = NULL;
   }
}

/** \brief Handler for start element events. */
static void
optInfoStartElem(void *userData, const char *name, const char **attr)
{
   struct OptInfoData *data = (struct OptInfoData *)userData;
   enum OptInfoElem elem = bsearchStr (name, OptInfoElems, OI_COUNT);
   switch (elem) {
   case OI_DRIINFO:
      if (data->inDriInfo)
         XML_FATAL1 ("nested <driinfo> elements.");
      if (attr[0])
         XML_FATAL1 ("attributes specified on <driinfo> element.");
      data->inDriInfo = true;
      break;
   case OI_SECTION:
      if (!data->inDriInfo)
         XML_FATAL1 ("<section> must be inside <driinfo>.");
      if (data->inSection)
         XML_FATAL1 ("nested <section> elements.");
      if (attr[0])
         XML_FATAL1 ("attributes specified on <section> element.");
      data->inSection = true;
      break;
   case OI_DESCRIPTION:
      if (!data->inSection && !data->inOption)
         XML_FATAL1 ("<description> must be inside <description> or <option.");
      if (data->inDesc)
         XML_FATAL1 ("nested <description> elements.");
      data->inDesc = true;
      parseDescAttr (data, attr);
      break;
   case OI_OPTION:
      if (!data->inSection)
         XML_FATAL1 ("<option> must be inside <section>.");
      if (data->inDesc)
         XML_FATAL1 ("<option> nested in <description> element.");
      if (data->inOption)
         XML_FATAL1 ("nested <option> elements.");
      data->inOption = true;
      parseOptInfoAttr (data, attr);
      break;
   case OI_ENUM:
      if (!(data->inOption && data->inDesc))
         XML_FATAL1 ("<enum> must be inside <option> and <description>.");
      if (data->inEnum)
         XML_FATAL1 ("nested <enum> elements.");
      data->inEnum = true;
      parseEnumAttr (data, attr);
      break;
   default:
      XML_FATAL ("unknown element: %s.", name);
   }
}

/** \brief Handler for end element events. */
static void
optInfoEndElem(void *userData, const char *name)
{
   struct OptInfoData *data = (struct OptInfoData *)userData;
   enum OptInfoElem elem = bsearchStr (name, OptInfoElems, OI_COUNT);
   switch (elem) {
   case OI_DRIINFO:
      data->inDriInfo = false;
      break;
   case OI_SECTION:
      data->inSection = false;
      break;
   case OI_DESCRIPTION:
      data->inDesc = false;
      break;
   case OI_OPTION:
      data->inOption = false;
      break;
   case OI_ENUM:
      data->inEnum = false;
      break;
   default:
      assert (0); /* should have been caught by StartElem */
   }
}

#else /* !XMLCONFIG */

static char *
next_field(void *mem_ctx, const char **configp)
{
   const char *config = *configp;

   int field_len;
   const char *sep = strchr(config, ',');
   if (sep) {
      field_len = sep - config;
      *configp = config + field_len + 1;
   } else {
      field_len = strlen(config);
      *configp = config + field_len;
   }

   if (!field_len)
      return NULL;

   return ralloc_strndup(mem_ctx, config, field_len);
}

static void
notxml_parse(driOptionCache *info, const char *configOptions)
{
   void *mem_ctx = info->info;
   char *name;
   while ((name = next_field(mem_ctx, &configOptions))) {
      driOptionInfo *opt = rzalloc(mem_ctx, driOptionInfo);
      opt->name = name;

      char *typestr = next_field(mem_ctx, &configOptions);
      if (!lookupType(typestr, &opt->type)) {
         fprintf (stderr, "Failed to parse type '%s'", typestr);
         abort();
      }
      ralloc_free(typestr);

      _mesa_hash_table_insert(info->info, opt->name, opt);

      driOptionValue *val = lookupValue(info, opt->name);
      char *valstr = next_field(mem_ctx, &configOptions);
      if (!parseValue(val, opt->type, valstr)) {
         fprintf (stderr, "Failed to parse value '%s'", valstr);
         abort();
      }
      ralloc_free(valstr);
   }
}

#endif /* !XMLCONFIG */

void
driParseOptionInfo(driOptionCache *info, const char *configOptions)
{
   /* Make the hash table big enough to fit more than the maximum number of
    * config options we've ever seen in a driver.
    */
   info->info = _mesa_hash_table_create(NULL,
                                        _mesa_hash_string,
                                        _mesa_key_string_equal);
   info->values = _mesa_hash_table_create(NULL,
                                          _mesa_hash_string,
                                          _mesa_key_string_equal);
   if (info->info == NULL || info->values == NULL) {
      fprintf (stderr, "%s: %d: out of memory.\n", __FILE__, __LINE__);
      abort();
   }

#if XMLCONFIG
   struct OptInfoData userData;
   struct OptInfoData *data = &userData;

   XML_Parser p = XML_ParserCreate ("UTF-8"); /* always UTF-8 */
   XML_SetElementHandler (p, optInfoStartElem, optInfoEndElem);
   XML_SetUserData (p, data);

   userData.name = "__driConfigOptions";
   userData.parser = p;
   userData.cache = info;
   userData.inDriInfo = false;
   userData.inSection = false;
   userData.inDesc = false;
   userData.inOption = false;
   userData.inEnum = false;
   userData.curOption = NULL;

   int status = XML_Parse (p, configOptions, strlen (configOptions), 1);
   if (!status)
      XML_FATAL ("%s.", XML_ErrorString(XML_GetErrorCode(p)));

   XML_ParserFree (p);
#else /* !XMLCONFIG */
   notxml_parse(info, configOptions);
#endif /* !XMLCONFIG */
}

#if XMLCONFIG
/** \brief Parser context for configuration files. */
struct OptConfData {
   const char *name;
   XML_Parser parser;
   driOptionCache *cache;
   int screenNum;
   const char *driverName, *execName;
   const char *kernelDriverName;
   const char *engineName;
   const char *applicationName;
   uint32_t engineVersion;
   uint32_t applicationVersion;
   uint32_t ignoringDevice;
   uint32_t ignoringApp;
   uint32_t inDriConf;
   uint32_t inDevice;
   uint32_t inApp;
   uint32_t inOption;
};

/** \brief Elements in configuration files. */
enum OptConfElem {
   OC_APPLICATION = 0, OC_DEVICE, OC_DRICONF, OC_ENGINE, OC_OPTION, OC_COUNT
};
static const char *OptConfElems[] = {
   [OC_APPLICATION]  = "application",
   [OC_DEVICE] = "device",
   [OC_DRICONF] = "driconf",
   [OC_ENGINE]  = "engine",
   [OC_OPTION] = "option",
};

/** \brief Parse attributes of a device element. */
static void
parseDeviceAttr(struct OptConfData *data, const char **attr)
{
   uint32_t i;
   const char *driver = NULL, *screen = NULL, *kernel = NULL;
   for (i = 0; attr[i]; i += 2) {
      if (!strcmp (attr[i], "driver")) driver = attr[i+1];
      else if (!strcmp (attr[i], "screen")) screen = attr[i+1];
      else if (!strcmp (attr[i], "kernel_driver")) kernel = attr[i+1];
      else XML_WARNING("unknown device attribute: %s.", attr[i]);
   }
   if (driver && strcmp (driver, data->driverName))
      data->ignoringDevice = data->inDevice;
   else if (kernel && (!data->kernelDriverName || strcmp (kernel, data->kernelDriverName)))
      data->ignoringDevice = data->inDevice;
   else if (screen) {
      driOptionValue screenNum;
      if (!parseValue (&screenNum, DRI_INT, screen))
         XML_WARNING("illegal screen number: %s.", screen);
      else if (screenNum._int != data->screenNum)
         data->ignoringDevice = data->inDevice;
   }
}

static bool
valueInRanges(const driOptionInfo *info, uint32_t value)
{
   uint32_t i;

   for (i = 0; i < info->nRanges; i++) {
      if (info->ranges[i].start._int <= value &&
          info->ranges[i].end._int >= value)
         return true;
   }

   return false;
}

/** \brief Parse attributes of an application element. */
static void
parseAppAttr(struct OptConfData *data, const char **attr)
{
   uint32_t i;
   const char *exec = NULL;
   const char *sha1 = NULL;
   const char *application_name_match = NULL;
   const char *application_versions = NULL;
   driOptionInfo *version_ranges = rzalloc(NULL, driOptionInfo);
   version_ranges->type = DRI_INT;

   for (i = 0; attr[i]; i += 2) {
      if (!strcmp (attr[i], "name")) /* not needed here */;
      else if (!strcmp (attr[i], "executable")) exec = attr[i+1];
      else if (!strcmp (attr[i], "sha1")) sha1 = attr[i+1];
      else if (!strcmp (attr[i], "application_name_match"))
         application_name_match = attr[i+1];
      else if (!strcmp (attr[i], "application_versions"))
         application_versions = attr[i+1];
      else XML_WARNING("unknown application attribute: %s.", attr[i]);
   }
   if (exec && strcmp (exec, data->execName)) {
      data->ignoringApp = data->inApp;
   } else if (sha1) {
      /* SHA1_DIGEST_STRING_LENGTH includes terminating null byte */
      if (strlen(sha1) != (SHA1_DIGEST_STRING_LENGTH - 1)) {
         XML_WARNING("Incorrect sha1 application attribute");
         data->ignoringApp = data->inApp;
      } else {
         size_t len;
         char* content;
         char path[PATH_MAX];
         if (util_get_process_exec_path(path, ARRAY_SIZE(path)) > 0 &&
             (content = os_read_file(path, &len))) {
            uint8_t sha1x[SHA1_DIGEST_LENGTH];
            char sha1s[SHA1_DIGEST_STRING_LENGTH];
            _mesa_sha1_compute(content, len, sha1x);
            _mesa_sha1_format((char*) sha1s, sha1x);
            free(content);

            if (strcmp(sha1, sha1s)) {
               data->ignoringApp = data->inApp;
            }
         } else {
            data->ignoringApp = data->inApp;
         }
      }
   } else if (application_name_match) {
      regex_t re;

      if (regcomp (&re, application_name_match, REG_EXTENDED|REG_NOSUB) == 0) {
         if (regexec (&re, data->applicationName, 0, NULL, 0) == REG_NOMATCH)
            data->ignoringApp = data->inApp;
         regfree (&re);
      } else
         XML_WARNING ("Invalid application_name_match=\"%s\".", application_name_match);
   }
   if (application_versions) {
      if (parseRanges (version_ranges, application_versions) &&
          !valueInRanges (version_ranges, data->applicationVersion))
         data->ignoringApp = data->inApp;
   }
   ralloc_free(version_ranges);
}

/** \brief Parse attributes of an application element. */
static void
parseEngineAttr(struct OptConfData *data, const char **attr)
{
   uint32_t i;
   const char *engine_name_match = NULL, *engine_versions = NULL;
   driOptionInfo *version_ranges = rzalloc(NULL, driOptionInfo);
   version_ranges->type = DRI_INT;

   for (i = 0; attr[i]; i += 2) {
      if (!strcmp (attr[i], "name")) /* not needed here */;
      else if (!strcmp (attr[i], "engine_name_match")) engine_name_match = attr[i+1];
      else if (!strcmp (attr[i], "engine_versions")) engine_versions = attr[i+1];
      else XML_WARNING("unknown application attribute: %s.", attr[i]);
   }
   if (engine_name_match) {
      regex_t re;

      if (regcomp (&re, engine_name_match, REG_EXTENDED|REG_NOSUB) == 0) {
         if (regexec (&re, data->engineName, 0, NULL, 0) == REG_NOMATCH)
            data->ignoringApp = data->inApp;
         regfree (&re);
      } else
         XML_WARNING ("Invalid engine_name_match=\"%s\".", engine_name_match);
   }
   if (engine_versions) {
      if (parseRanges (version_ranges, engine_versions) &&
          !valueInRanges (version_ranges, data->engineVersion))
         data->ignoringApp = data->inApp;
   }

   ralloc_free(version_ranges);
}

/** \brief Parse attributes of an option element. */
static void
parseOptConfAttr(struct OptConfData *data, const char **attr)
{
   uint32_t i;
   const char *name = NULL, *value = NULL;
   for (i = 0; attr[i]; i += 2) {
      if (!strcmp (attr[i], "name")) name = attr[i+1];
      else if (!strcmp (attr[i], "value")) value = attr[i+1];
      else XML_WARNING("unknown option attribute: %s.", attr[i]);
   }
   if (!name) XML_WARNING1 ("name attribute missing in option.");
   if (!value) XML_WARNING1 ("value attribute missing in option.");
   if (name && value) {
      driOptionCache *cache = data->cache;
      driOptionInfo *opt = lookupInfo (cache, name);
      if (!opt)
         /* don't use XML_WARNING, drirc defines options for all drivers,
          * but not all drivers support them */
         return;
      else if (getenv (opt->name)) {
         /* don't use XML_WARNING, we want the user to see this! */
         if (be_verbose()) {
            fprintf(stderr,
                    "ATTENTION: option value of option %s ignored.\n",
                    opt->name);
         }
      } else if (!parseValue (lookupValue(data->cache, opt->name),
                              opt->type, value))
         XML_WARNING ("illegal option value: %s.", value);
   }
}

/** \brief Handler for start element events. */
static void
optConfStartElem(void *userData, const char *name,
                 const char **attr)
{
   struct OptConfData *data = (struct OptConfData *)userData;
   enum OptConfElem elem = bsearchStr (name, OptConfElems, OC_COUNT);
   switch (elem) {
   case OC_DRICONF:
      if (data->inDriConf)
         XML_WARNING1 ("nested <driconf> elements.");
      if (attr[0])
         XML_WARNING1 ("attributes specified on <driconf> element.");
      data->inDriConf++;
      break;
   case OC_DEVICE:
      if (!data->inDriConf)
         XML_WARNING1 ("<device> should be inside <driconf>.");
      if (data->inDevice)
         XML_WARNING1 ("nested <device> elements.");
      data->inDevice++;
      if (!data->ignoringDevice && !data->ignoringApp)
         parseDeviceAttr (data, attr);
      break;
   case OC_APPLICATION:
      if (!data->inDevice)
         XML_WARNING1 ("<application> should be inside <device>.");
      if (data->inApp)
         XML_WARNING1 ("nested <application> or <engine> elements.");
      data->inApp++;
      if (!data->ignoringDevice && !data->ignoringApp)
         parseAppAttr (data, attr);
      break;
   case OC_ENGINE:
      if (!data->inDevice)
         XML_WARNING1 ("<engine> should be inside <device>.");
      if (data->inApp)
         XML_WARNING1 ("nested <application> or <engine> elements.");
      data->inApp++;
      if (!data->ignoringDevice && !data->ignoringApp)
         parseEngineAttr (data, attr);
      break;
   case OC_OPTION:
      if (!data->inApp)
         XML_WARNING1 ("<option> should be inside <application>.");
      if (data->inOption)
         XML_WARNING1 ("nested <option> elements.");
      data->inOption++;
      if (!data->ignoringDevice && !data->ignoringApp)
         parseOptConfAttr (data, attr);
      break;
   default:
      XML_WARNING ("unknown element: %s.", name);
   }
}

/** \brief Handler for end element events. */
static void
optConfEndElem(void *userData, const char *name)
{
   struct OptConfData *data = (struct OptConfData *)userData;
   enum OptConfElem elem = bsearchStr (name, OptConfElems, OC_COUNT);
   switch (elem) {
   case OC_DRICONF:
      data->inDriConf--;
      break;
   case OC_DEVICE:
      if (data->inDevice-- == data->ignoringDevice)
         data->ignoringDevice = 0;
      break;
   case OC_APPLICATION:
   case OC_ENGINE:
      if (data->inApp-- == data->ignoringApp)
         data->ignoringApp = 0;
      break;
   case OC_OPTION:
      data->inOption--;
      break;
   default:
      /* unknown element, warning was produced on start tag */;
   }
}
#endif /* XMLCONFIG */

/** \brief Initialize an option cache based on info */
static void
initOptionCache(driOptionCache *cache, const driOptionCache *info)
{
   cache->info = info->info;
   cache->values = _mesa_hash_table_create(NULL,
                                           _mesa_hash_string,
                                           _mesa_key_string_equal);
   ALLOC_CHECK(cache->values);

   hash_table_foreach(info->info, entry) {
      driOptionInfo *opt = entry->data;
      driOptionValue *src = lookupValue(info, opt->name);
      driOptionValue *dst = lookupValue(cache, opt->name);
      if (opt->type == DRI_STRING) {
         dst->_string = ralloc_strdup(cache->values, src->_string);
      } else {
         *dst = *src;
      }
   }
}

#if XMLCONFIG
static void
_parseOneConfigFile(XML_Parser p)
{
#define BUF_SIZE 0x1000
   struct OptConfData *data = (struct OptConfData *)XML_GetUserData (p);
   int status;
   int fd;

   if ((fd = open (data->name, O_RDONLY)) == -1) {
      __driUtilMessage ("Can't open configuration file %s: %s.",
                        data->name, strerror (errno));
      return;
   }

   while (1) {
      int bytesRead;
      void *buffer = XML_GetBuffer (p, BUF_SIZE);
      if (!buffer) {
         __driUtilMessage ("Can't allocate parser buffer.");
         break;
      }
      bytesRead = read (fd, buffer, BUF_SIZE);
      if (bytesRead == -1) {
         __driUtilMessage ("Error reading from configuration file %s: %s.",
                           data->name, strerror (errno));
         break;
      }
      status = XML_ParseBuffer (p, bytesRead, bytesRead == 0);
      if (!status) {
         XML_ERROR ("%s.", XML_ErrorString(XML_GetErrorCode(p)));
         break;
      }
      if (bytesRead == 0)
         break;
   }

   close (fd);
#undef BUF_SIZE
}

/** \brief Parse the named configuration file */
static void
parseOneConfigFile(struct OptConfData *data, const char *filename)
{
   XML_Parser p;

   p = XML_ParserCreate (NULL); /* use encoding specified by file */
   XML_SetElementHandler (p, optConfStartElem, optConfEndElem);
   XML_SetUserData (p, data);
   data->parser = p;
   data->name = filename;
   data->ignoringDevice = 0;
   data->ignoringApp = 0;
   data->inDriConf = 0;
   data->inDevice = 0;
   data->inApp = 0;
   data->inOption = 0;

   _parseOneConfigFile (p);
   XML_ParserFree (p);
}

static int
scandir_filter(const struct dirent *ent)
{
#ifndef DT_REG /* systems without d_type in dirent results */
   struct stat st;

   if ((lstat(ent->d_name, &st) != 0) ||
       (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode)))
      return 0;
#else
   if (ent->d_type != DT_REG && ent->d_type != DT_LNK)
      return 0;
#endif

    int len = strlen(ent->d_name);
    if (len > 5 && strcmp(ent->d_name + len - 5, ".conf") == 0)
       return 0;

   return 1;
}

/** \brief Parse configuration files in a directory */
static void
parseConfigDir(struct OptConfData *data, const char *dirname)
{
   int i, count;
   struct dirent **entries = NULL;

   count = scandir(dirname, &entries, scandir_filter, alphasort);
   if (count < 0)
      return;

   for (i = 0; i < count; i++) {
      char filename[PATH_MAX];

      snprintf(filename, PATH_MAX, "%s/%s", dirname, entries[i]->d_name);
      free(entries[i]);

      parseOneConfigFile(data, filename);
   }

   free(entries);
}

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

#ifndef DATADIR
#define DATADIR "/usr/share"
#endif

#endif /* XMLCONFIG */

void
driParseConfigFiles(driOptionCache *cache, const driOptionCache *info,
                    int screenNum, const char *driverName,
                    const char *kernelDriverName,
                    const char *applicationName, uint32_t applicationVersion,
                    const char *engineName, uint32_t engineVersion)
{
   initOptionCache (cache, info);

#if XMLCONFIG
   struct OptConfData userData;

   userData.cache = cache;
   userData.screenNum = screenNum;
   userData.driverName = driverName;
   userData.kernelDriverName = kernelDriverName;
   userData.applicationName = applicationName ? applicationName : "";
   userData.applicationVersion = applicationVersion;
   userData.engineName = engineName ? engineName : "";
   userData.engineVersion = engineVersion;
   userData.execName = util_get_process_name();

   parseConfigDir(&userData, DATADIR "/drirc.d");
   parseOneConfigFile(&userData, SYSCONFDIR "/drirc");

   char *home = getenv ("HOME");
   if (home) {
      char filename[PATH_MAX];

      snprintf(filename, PATH_MAX, "%s/.drirc", home);
      parseOneConfigFile(&userData, filename);
   }
#endif /* XMLCONFIG */
}

void
driDestroyOptionInfo(driOptionCache *info)
{
   driDestroyOptionCache(info);
   ralloc_free(info->info);
}

void
driDestroyOptionCache(driOptionCache *cache)
{
   ralloc_free(cache->values);
}

unsigned char
driCheckOption(const driOptionCache *cache, const char *name,
               driOptionType type)
{
   const driOptionInfo *info = lookupInfo(cache, name);
   return info && info->type == type;
}

unsigned char
driQueryOptionb(const driOptionCache *cache, const char *name)
{
   /* make sure the option is defined and has the correct type */
   assert(driCheckOption(cache, name, DRI_BOOL));
   return lookupValue(cache, name)->_bool;
}

int
driQueryOptioni(const driOptionCache *cache, const char *name)
{
   /* make sure the option is defined and has the correct type */
   assert(lookupInfo(cache, name));
   assert(lookupInfo(cache, name)->type == DRI_INT ||
          lookupInfo(cache, name)->type == DRI_ENUM);
   return lookupValue(cache, name)->_int;
}

float
driQueryOptionf(const driOptionCache *cache, const char *name)
{
   /* make sure the option is defined and has the correct type */
   assert(driCheckOption(cache, name, DRI_FLOAT));
   return lookupValue(cache, name)->_float;
}

char *
driQueryOptionstr(const driOptionCache *cache, const char *name)
{
   /* make sure the option is defined and has the correct type */
   assert(driCheckOption(cache, name, DRI_STRING));
   return lookupValue(cache, name)->_string;
}

/**
 * Returns a hash of the options for this application.
 */
void
driComputeOptionsSha1(const driOptionCache *cache, unsigned char *sha1)
{
   void *ctx = ralloc_context(NULL);
   char *dri_options = ralloc_strdup(ctx, "");

   /* Note that the table is hashed by string key contents, so it will be
    * stable when walking
    */
   hash_table_foreach(cache->info, entry) {
      const driOptionInfo *opt = entry->data;
      const driOptionValue *value = lookupValue(cache, opt->name);

      bool ret = false;
      switch (opt->type) {
      case DRI_BOOL:
         ret = ralloc_asprintf_append(&dri_options, "%s:%u,",
                                      opt->name, value->_bool);
         break;
      case DRI_INT:
      case DRI_ENUM:
         ret = ralloc_asprintf_append(&dri_options, "%s:%d,",
                                      opt->name, value->_int);
         break;
      case DRI_FLOAT:
         ret = ralloc_asprintf_append(&dri_options, "%s:%f,",
                                      opt->name, value->_float);
         break;
      case DRI_STRING:
         ret = ralloc_asprintf_append(&dri_options, "%s:%s,",
                                      opt->name, value->_string);
         break;
      default:
         unreachable("unsupported dri config type!");
      }

      if (!ret) {
         break;
      }
   }

   _mesa_sha1_compute(dri_options, strlen(dri_options), sha1);
   ralloc_free(ctx);
}
