/* soapStub.h
   Generated by gSOAP 2.8.8 from xml-rpc.h

Copyright(C) 2000-2012, Robert van Engelen, Genivia Inc. All Rights Reserved.
The generated code is released under one of the following licenses:
1) GPL or 2) Genivia's license for commercial use.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
*/

#ifndef soapStub_H
#define soapStub_H
#include "xml-rpc-iters.h"	// deferred to C++ compiler
#include "stdsoap2.h"
#if GSOAP_VERSION != 20808
# error "GSOAP VERSION MISMATCH IN GENERATED CODE: PLEASE REINSTALL PACKAGE"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
 *                                                                            *
 * Enumerations                                                               *
 *                                                                            *
\******************************************************************************/


/******************************************************************************\
 *                                                                            *
 * Types with Custom Serializers                                              *
 *                                                                            *
\******************************************************************************/


/******************************************************************************\
 *                                                                            *
 * Classes and Structs                                                        *
 *                                                                            *
\******************************************************************************/


#if 0 /* volatile type: do not declare here, declared elsewhere */

#endif







#ifndef SOAP_TYPE__base64
#define SOAP_TYPE__base64 (18)
/* Base64 schema type: */
struct _base64
{
	unsigned char *__ptr;
	int __size;
};
#endif

#ifndef SOAP_TYPE__struct
#define SOAP_TYPE__struct (35)
/* struct */
struct _struct
{
	int __size;	/* sequence of elements <member> */
	struct member *member;	/* optional element of type member */
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_data
#define SOAP_TYPE_data (51)
/* data */
struct data
{
	int __size;	/* sequence of elements <value> */
	struct value *value;	/* optional element of type value */
};
#endif

#ifndef SOAP_TYPE__array
#define SOAP_TYPE__array (53)
/* array */
struct _array
{
	struct data data;	/* required element of type data */
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_value
#define SOAP_TYPE_value (42)
/* value */
struct value
{
	int __type;	/* any type of element <ref> (defined below) */
	void *ref;	/* transient */
	char *__any;
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_member
#define SOAP_TYPE_member (48)
/* member */
struct member
{
	char *name;	/* optional element of type xsd:string */
	struct value value;	/* required element of type value */
};
#endif

#ifndef SOAP_TYPE_params
#define SOAP_TYPE_params (117)
/* params */
struct params
{
	int __size;	/* sequence of elements <param> */
	struct param *param;	/* optional element of type param */
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_param
#define SOAP_TYPE_param (126)
/* param */
struct param
{
	struct value value;	/* required element of type value */
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_methodResponse
#define SOAP_TYPE_methodResponse (128)
/* methodResponse */
struct methodResponse
{
	struct params *params;	/* optional element of type params */
	struct fault *fault;	/* optional element of type fault */
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_methodCall
#define SOAP_TYPE_methodCall (141)
/* methodCall */
struct methodCall
{
	char *methodEndpoint;	/* not serialized */
	struct methodResponse *methodResponse;	/* not serialized */
	char *methodName;	/* optional element of type xsd:string */
	struct params params;	/* required element of type params */
	struct soap *soap;	/* transient */
};
#endif

#ifndef SOAP_TYPE_fault
#define SOAP_TYPE_fault (139)
/* fault */
struct fault
{
	struct value value;	/* required element of type value */
};
#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Header
#define SOAP_TYPE_SOAP_ENV__Header (157)
/* SOAP Header: */
struct SOAP_ENV__Header
{
#ifdef WITH_NOEMPTYSTRUCT
	char dummy;	/* dummy member to enable compilation */
#endif
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Code
#define SOAP_TYPE_SOAP_ENV__Code (158)
/* SOAP Fault Code: */
struct SOAP_ENV__Code
{
	char *SOAP_ENV__Value;	/* optional element of type xsd:QName */
	struct SOAP_ENV__Code *SOAP_ENV__Subcode;	/* optional element of type SOAP-ENV:Code */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Detail
#define SOAP_TYPE_SOAP_ENV__Detail (160)
/* SOAP-ENV:Detail */
struct SOAP_ENV__Detail
{
	char *__any;
	int __type;	/* any type of element <fault> (defined below) */
	void *fault;	/* transient */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Reason
#define SOAP_TYPE_SOAP_ENV__Reason (161)
/* SOAP-ENV:Reason */
struct SOAP_ENV__Reason
{
	char *SOAP_ENV__Text;	/* optional element of type xsd:string */
};
#endif

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Fault
#define SOAP_TYPE_SOAP_ENV__Fault (162)
/* SOAP Fault: */
struct SOAP_ENV__Fault
{
	char *faultcode;	/* optional element of type xsd:QName */
	char *faultstring;	/* optional element of type xsd:string */
	char *faultactor;	/* optional element of type xsd:string */
	struct SOAP_ENV__Detail *detail;	/* optional element of type SOAP-ENV:Detail */
	struct SOAP_ENV__Code *SOAP_ENV__Code;	/* optional element of type SOAP-ENV:Code */
	struct SOAP_ENV__Reason *SOAP_ENV__Reason;	/* optional element of type SOAP-ENV:Reason */
	char *SOAP_ENV__Node;	/* optional element of type xsd:string */
	char *SOAP_ENV__Role;	/* optional element of type xsd:string */
	struct SOAP_ENV__Detail *SOAP_ENV__Detail;	/* optional element of type SOAP-ENV:Detail */
};
#endif

#endif

/******************************************************************************\
 *                                                                            *
 * Typedefs                                                                   *
 *                                                                            *
\******************************************************************************/

#ifndef SOAP_TYPE__QName
#define SOAP_TYPE__QName (5)
typedef char *_QName;
#endif

#ifndef SOAP_TYPE__XML
#define SOAP_TYPE__XML (6)
typedef char *_XML;
#endif

#ifndef SOAP_TYPE__boolean
#define SOAP_TYPE__boolean (11)
typedef char _boolean;
#endif

#ifndef SOAP_TYPE__double
#define SOAP_TYPE__double (13)
typedef double _double;
#endif

#ifndef SOAP_TYPE__i4
#define SOAP_TYPE__i4 (14)
typedef int _i4;
#endif

#ifndef SOAP_TYPE__int
#define SOAP_TYPE__int (15)
typedef int _int;
#endif

#ifndef SOAP_TYPE__string
#define SOAP_TYPE__string (16)
typedef char *_string;
#endif

#ifndef SOAP_TYPE__dateTime_DOTiso8601
#define SOAP_TYPE__dateTime_DOTiso8601 (17)
typedef char *_dateTime_DOTiso8601;
#endif


/******************************************************************************\
 *                                                                            *
 * Externals                                                                  *
 *                                                                            *
\******************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

/* End of soapStub.h */
