/* soapStub.h
   Generated by gSOAP 2.8.45 for primes.h

gSOAP XML Web services tools
Copyright (C) 2000-2017, Robert van Engelen, Genivia Inc. All Rights Reserved.
The soapcpp2 tool and its generated software are released under the GPL.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
--------------------------------------------------------------------------------
A commercial use license is available from Genivia Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

#include "simple_vector.h" // defines simple_vector<>

#ifndef soapStub_H
#define soapStub_H
#include "stdsoap2.h"
#if GSOAP_VERSION != 20845
# error "GSOAP VERSION 20845 MISMATCH IN GENERATED CODE VERSUS LIBRARY CODE: PLEASE REINSTALL PACKAGE"
#endif


/******************************************************************************\
 *                                                                            *
 * Types with Custom Serializers                                              *
 *                                                                            *
\******************************************************************************/


/******************************************************************************\
 *                                                                            *
 * Classes, Structs and Unions                                                *
 *                                                                            *
\******************************************************************************/

class primes;	/* primes.h:39 */

/* primes.h:39 */
#ifndef SOAP_TYPE_primes
#define SOAP_TYPE_primes (8)
/* Type primes is a recursive data type, (in)directly referencing itself through its (base or derived class) members */
/* complex XSD type 'primes': */
class SOAP_CMAC primes : public soap {
      public:
        /// Optional element 'prime' of XSD type 'xsd:int'
        simple_vector<int> prime;
        void sieve(int n);
        void write();
        virtual ~primes();
      public:
        /// Return unique type id SOAP_TYPE_primes
        virtual int soap_type(void) const { return SOAP_TYPE_primes; }
        /// (Re)set members to default values
        virtual void soap_default(struct soap*);
        /// Serialize object to prepare for SOAP 1.1/1.2 encoded output (or with SOAP_XML_GRAPH) by analyzing its (cyclic) structures
        virtual void soap_serialize(struct soap*) const;
        /// Output object in XML, compliant with SOAP 1.1 encoding style, return error code or SOAP_OK
        virtual int soap_put(struct soap*, const char *tag, const char *type) const;
        /// Output object in XML, with tag and optional id attribute and xsi:type, return error code or SOAP_OK
        virtual int soap_out(struct soap*, const char *tag, int id, const char *type) const;
        /// Get object from XML, compliant with SOAP 1.1 encoding style, return pointer to object or NULL on error
        virtual void *soap_get(struct soap*, const char *tag, const char *type);
        /// Get object from XML, with matching tag and type (NULL matches any tag and type), return pointer to object or NULL on error
        virtual void *soap_in(struct soap*, const char *tag, const char *type);
        /// Return a new object of type primes, default initialized and not managed by a soap context
        virtual primes *soap_alloc(void) const { return SOAP_NEW(primes); }
      public:
        /// Constructor with initializations
        primes()
        {
        }
        /// Friend allocator used by soap_new_primes(struct soap*, int)
        friend SOAP_FMAC1 primes * SOAP_FMAC2 soap_instantiate_primes(struct soap*, int, const char*, const char*, size_t*);
};
#endif

/* primes.h:49 */
#ifndef WITH_NOGLOBAL
#ifndef SOAP_TYPE_SOAP_ENV__Header
#define SOAP_TYPE_SOAP_ENV__Header (16)
/* SOAP_ENV__Header: */
struct SOAP_CMAC SOAP_ENV__Header {
      public:
        /** Return unique type id SOAP_TYPE_SOAP_ENV__Header */
        int soap_type() const { return SOAP_TYPE_SOAP_ENV__Header; }
        /** Constructor with member initializations */
        SOAP_ENV__Header()
        {
        }
        /** Friend allocator used by soap_new_SOAP_ENV__Header(struct soap*, int) */
        friend SOAP_FMAC1 SOAP_ENV__Header * SOAP_FMAC2 soap_instantiate_SOAP_ENV__Header(struct soap*, int, const char*, const char*, size_t*);
};
#endif
#endif

/* primes.h:49 */
#ifndef WITH_NOGLOBAL
#ifndef SOAP_TYPE_SOAP_ENV__Code
#define SOAP_TYPE_SOAP_ENV__Code (17)
/* Type SOAP_ENV__Code is a recursive data type, (in)directly referencing itself through its (base or derived class) members */
/* SOAP_ENV__Code: */
struct SOAP_CMAC SOAP_ENV__Code {
      public:
        /** Optional element 'SOAP-ENV:Value' of XSD type 'xsd:QName' */
        char *SOAP_ENV__Value;
        /** Optional element 'SOAP-ENV:Subcode' of XSD type 'SOAP-ENV:Code' */
        struct SOAP_ENV__Code *SOAP_ENV__Subcode;
      public:
        /** Return unique type id SOAP_TYPE_SOAP_ENV__Code */
        int soap_type() const { return SOAP_TYPE_SOAP_ENV__Code; }
        /** Constructor with member initializations */
        SOAP_ENV__Code()
        {
          SOAP_ENV__Value = (char *)0;
          SOAP_ENV__Subcode = (struct SOAP_ENV__Code *)0;
        }
        /** Friend allocator used by soap_new_SOAP_ENV__Code(struct soap*, int) */
        friend SOAP_FMAC1 SOAP_ENV__Code * SOAP_FMAC2 soap_instantiate_SOAP_ENV__Code(struct soap*, int, const char*, const char*, size_t*);
};
#endif
#endif

/* primes.h:49 */
#ifndef WITH_NOGLOBAL
#ifndef SOAP_TYPE_SOAP_ENV__Detail
#define SOAP_TYPE_SOAP_ENV__Detail (19)
/* SOAP_ENV__Detail: */
struct SOAP_CMAC SOAP_ENV__Detail {
      public:
        char *__any;
        /** Any type of element 'fault' assigned to fault with its SOAP_TYPE_T assigned to __type */
        /** Do not create a cyclic data structure throught this member unless SOAP encoding or SOAP_XML_GRAPH are used for id-ref serialization */
        int __type;
        void *fault;
      public:
        /** Return unique type id SOAP_TYPE_SOAP_ENV__Detail */
        int soap_type() const { return SOAP_TYPE_SOAP_ENV__Detail; }
        /** Constructor with member initializations */
        SOAP_ENV__Detail()
        {
          __any = (char *)0;
          __type = 0;
          fault = NULL;
        }
        /** Friend allocator used by soap_new_SOAP_ENV__Detail(struct soap*, int) */
        friend SOAP_FMAC1 SOAP_ENV__Detail * SOAP_FMAC2 soap_instantiate_SOAP_ENV__Detail(struct soap*, int, const char*, const char*, size_t*);
};
#endif
#endif

/* primes.h:49 */
#ifndef WITH_NOGLOBAL
#ifndef SOAP_TYPE_SOAP_ENV__Reason
#define SOAP_TYPE_SOAP_ENV__Reason (21)
/* SOAP_ENV__Reason: */
struct SOAP_CMAC SOAP_ENV__Reason {
      public:
        /** Optional element 'SOAP-ENV:Text' of XSD type 'xsd:string' */
        char *SOAP_ENV__Text;
      public:
        /** Return unique type id SOAP_TYPE_SOAP_ENV__Reason */
        int soap_type() const { return SOAP_TYPE_SOAP_ENV__Reason; }
        /** Constructor with member initializations */
        SOAP_ENV__Reason()
        {
          SOAP_ENV__Text = (char *)0;
        }
        /** Friend allocator used by soap_new_SOAP_ENV__Reason(struct soap*, int) */
        friend SOAP_FMAC1 SOAP_ENV__Reason * SOAP_FMAC2 soap_instantiate_SOAP_ENV__Reason(struct soap*, int, const char*, const char*, size_t*);
};
#endif
#endif

/* primes.h:49 */
#ifndef WITH_NOGLOBAL
#ifndef SOAP_TYPE_SOAP_ENV__Fault
#define SOAP_TYPE_SOAP_ENV__Fault (22)
/* SOAP_ENV__Fault: */
struct SOAP_CMAC SOAP_ENV__Fault {
      public:
        /** Optional element 'faultcode' of XSD type 'xsd:QName' */
        char *faultcode;
        /** Optional element 'faultstring' of XSD type 'xsd:string' */
        char *faultstring;
        /** Optional element 'faultactor' of XSD type 'xsd:string' */
        char *faultactor;
        /** Optional element 'detail' of XSD type 'SOAP-ENV:Detail' */
        struct SOAP_ENV__Detail *detail;
        /** Optional element 'SOAP-ENV:Code' of XSD type 'SOAP-ENV:Code' */
        struct SOAP_ENV__Code *SOAP_ENV__Code;
        /** Optional element 'SOAP-ENV:Reason' of XSD type 'SOAP-ENV:Reason' */
        struct SOAP_ENV__Reason *SOAP_ENV__Reason;
        /** Optional element 'SOAP-ENV:Node' of XSD type 'xsd:string' */
        char *SOAP_ENV__Node;
        /** Optional element 'SOAP-ENV:Role' of XSD type 'xsd:string' */
        char *SOAP_ENV__Role;
        /** Optional element 'SOAP-ENV:Detail' of XSD type 'SOAP-ENV:Detail' */
        struct SOAP_ENV__Detail *SOAP_ENV__Detail;
      public:
        /** Return unique type id SOAP_TYPE_SOAP_ENV__Fault */
        int soap_type() const { return SOAP_TYPE_SOAP_ENV__Fault; }
        /** Constructor with member initializations */
        SOAP_ENV__Fault()
        {
          faultcode = (char *)0;
          faultstring = (char *)0;
          faultactor = (char *)0;
          detail = (struct SOAP_ENV__Detail *)0;
          SOAP_ENV__Code = (struct SOAP_ENV__Code *)0;
          SOAP_ENV__Reason = (struct SOAP_ENV__Reason *)0;
          SOAP_ENV__Node = (char *)0;
          SOAP_ENV__Role = (char *)0;
          SOAP_ENV__Detail = (struct SOAP_ENV__Detail *)0;
        }
        /** Friend allocator used by soap_new_SOAP_ENV__Fault(struct soap*, int) */
        friend SOAP_FMAC1 SOAP_ENV__Fault * SOAP_FMAC2 soap_instantiate_SOAP_ENV__Fault(struct soap*, int, const char*, const char*, size_t*);
};
#endif
#endif

/******************************************************************************\
 *                                                                            *
 * Typedefs                                                                   *
 *                                                                            *
\******************************************************************************/


/* primes.h:36 */
#ifndef SOAP_TYPE__QName
#define SOAP_TYPE__QName (5)
typedef char *_QName;
#endif

/* primes.h:36 */
#ifndef SOAP_TYPE__XML
#define SOAP_TYPE__XML (6)
typedef char *_XML;
#endif

/******************************************************************************\
 *                                                                            *
 * Serializable Types                                                         *
 *                                                                            *
\******************************************************************************/


/* char has binding name 'byte' for type 'xsd:byte' */
#ifndef SOAP_TYPE_byte
#define SOAP_TYPE_byte (3)
#endif

/* int has binding name 'int' for type 'xsd:int' */
#ifndef SOAP_TYPE_int
#define SOAP_TYPE_int (1)
#endif

/* primes has binding name 'primes' for type 'primes' */
#ifndef SOAP_TYPE_primes
#define SOAP_TYPE_primes (8)
#endif

/* struct SOAP_ENV__Fault has binding name 'SOAP_ENV__Fault' for type '' */
#ifndef SOAP_TYPE_SOAP_ENV__Fault
#define SOAP_TYPE_SOAP_ENV__Fault (22)
#endif

/* struct SOAP_ENV__Reason has binding name 'SOAP_ENV__Reason' for type '' */
#ifndef SOAP_TYPE_SOAP_ENV__Reason
#define SOAP_TYPE_SOAP_ENV__Reason (21)
#endif

/* struct SOAP_ENV__Detail has binding name 'SOAP_ENV__Detail' for type '' */
#ifndef SOAP_TYPE_SOAP_ENV__Detail
#define SOAP_TYPE_SOAP_ENV__Detail (19)
#endif

/* struct SOAP_ENV__Code has binding name 'SOAP_ENV__Code' for type '' */
#ifndef SOAP_TYPE_SOAP_ENV__Code
#define SOAP_TYPE_SOAP_ENV__Code (17)
#endif

/* struct SOAP_ENV__Header has binding name 'SOAP_ENV__Header' for type '' */
#ifndef SOAP_TYPE_SOAP_ENV__Header
#define SOAP_TYPE_SOAP_ENV__Header (16)
#endif

/* struct SOAP_ENV__Reason * has binding name 'PointerToSOAP_ENV__Reason' for type '' */
#ifndef SOAP_TYPE_PointerToSOAP_ENV__Reason
#define SOAP_TYPE_PointerToSOAP_ENV__Reason (24)
#endif

/* struct SOAP_ENV__Detail * has binding name 'PointerToSOAP_ENV__Detail' for type '' */
#ifndef SOAP_TYPE_PointerToSOAP_ENV__Detail
#define SOAP_TYPE_PointerToSOAP_ENV__Detail (23)
#endif

/* struct SOAP_ENV__Code * has binding name 'PointerToSOAP_ENV__Code' for type '' */
#ifndef SOAP_TYPE_PointerToSOAP_ENV__Code
#define SOAP_TYPE_PointerToSOAP_ENV__Code (18)
#endif

/* _XML has binding name '_XML' for type '' */
#ifndef SOAP_TYPE__XML
#define SOAP_TYPE__XML (6)
#endif

/* _QName has binding name '_QName' for type 'xsd:QName' */
#ifndef SOAP_TYPE__QName
#define SOAP_TYPE__QName (5)
#endif

/* char * has binding name 'string' for type 'xsd:string' */
#ifndef SOAP_TYPE_string
#define SOAP_TYPE_string (4)
#endif

/* simple_vector<int>  has binding name 'simple_vectorTemplateOfint' for type 'xsd:int' */
#ifndef SOAP_TYPE_simple_vectorTemplateOfint
#define SOAP_TYPE_simple_vectorTemplateOfint (9)
#endif

/******************************************************************************\
 *                                                                            *
 * Externals                                                                  *
 *                                                                            *
\******************************************************************************/


#endif

/* End of soapStub.h */
