/*
	stdsoap2.c[pp] 2.8.26

	gSOAP runtime engine

gSOAP XML Web services tools
Copyright (C) 2000-2015, Robert van Engelen, Genivia Inc., All Rights Reserved.
This part of the software is released under ONE of the following licenses:
GPL, or the gSOAP public license, or Genivia's license for commercial use.
--------------------------------------------------------------------------------
Contributors:

Wind River Systems Inc., for the following additions under gSOAP public license:
  - vxWorks compatible options
--------------------------------------------------------------------------------
gSOAP public license.

The contents of this file are subject to the gSOAP Public License Version 1.3
(the "License"); you may not use this file except in compliance with the
License. You may obtain a copy of the License at
http://www.cs.fsu.edu/~engelen/soaplicense.html
Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the License.

The Initial Developer of the Original Code is Robert A. van Engelen.
Copyright (C) 2000-2014, Robert van Engelen, Genivia Inc., All Rights Reserved.
--------------------------------------------------------------------------------
GPL license.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

Author contact information:
engelen@genivia.com / engelen@acm.org

This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
--------------------------------------------------------------------------------
A commercial use license is available from Genivia, Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

#define GSOAP_LIB_VERSION 20826

#ifdef AS400
# pragma convert(819)	/* EBCDIC to ASCII */
#endif

#include "stdsoap2.h"

#if GSOAP_VERSION != GSOAP_LIB_VERSION
# error "GSOAP VERSION MISMATCH IN LIBRARY: PLEASE REINSTALL PACKAGE"
#endif

#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
# include <ipcom_key_db.h>
#endif

#ifdef __BORLANDC__
# pragma warn -8060
#else
# ifdef WIN32
#  ifdef UNDER_CE
#   pragma comment(lib, "ws2.lib")	/* WinCE */
#  else
#   pragma comment(lib, "Ws2_32.lib")
#  endif
#  pragma warning(disable : 4996) /* disable deprecation warnings */
# endif
#endif

#ifdef __cplusplus
SOAP_SOURCE_STAMP("@(#) stdsoap2.cpp ver 2.8.26 2015-11-30 00:00:00 GMT")
extern "C" {
#else
SOAP_SOURCE_STAMP("@(#) stdsoap2.c ver 2.8.26 2015-11-30 00:00:00 GMT")
#endif

/* 8bit character representing unknown character entity or multibyte data */
#ifndef SOAP_UNKNOWN_CHAR
# define SOAP_UNKNOWN_CHAR (0x7F)
#endif

/* unicode character representing unknown characters outside the XML 1.0 UTF8 unicode space */
#ifdef WITH_REPLACE_ILLEGAL_UTF8
# ifndef SOAP_UNKNOWN_UNICODE_CHAR
#  define SOAP_UNKNOWN_UNICODE_CHAR (0xFFFD)
# endif
#endif

/*      EOF=-1 */
#define SOAP_LT (soap_wchar)(-2) /* XML-specific '<' */
#define SOAP_TT (soap_wchar)(-3) /* XML-specific '</' */
#define SOAP_GT (soap_wchar)(-4) /* XML-specific '>' */
#define SOAP_QT (soap_wchar)(-5) /* XML-specific '"' */
#define SOAP_AP (soap_wchar)(-6) /* XML-specific ''' */

#define soap_blank(c)		((c)+1 > 0 && (c) <= 32)
#define soap_notblank(c)	((c) > 32)

#if defined(WIN32) && !defined(UNDER_CE)
#define soap_hash_ptr(p)	((PtrToUlong(p) >> 3) & (SOAP_PTRHASH - 1))
#else
#define soap_hash_ptr(p)	((size_t)(((unsigned long)(p) >> 3) & (SOAP_PTRHASH-1)))
#endif

#if !defined(WITH_LEAN) || defined(SOAP_DEBUG)
static void soap_init_logs(struct soap*);
#endif
#ifdef SOAP_DEBUG
static void soap_close_logfile(struct soap*, int);
static void soap_set_logfile(struct soap*, int, const char*);
#endif

#ifdef SOAP_MEM_DEBUG
static void soap_init_mht(struct soap*);
static void soap_free_mht(struct soap*);
static void soap_track_unlink(struct soap*, const void*);
#endif

#ifndef PALM_2
static int soap_set_error(struct soap*, const char*, const char*, const char*, const char*, int);
static int soap_copy_fault(struct soap*, const char*, const char*, const char*, const char*);
static int soap_getattrval(struct soap*, char*, size_t*, soap_wchar);
#endif

#ifndef PALM_1
static void soap_free_ns(struct soap *soap);
static soap_wchar soap_char(struct soap*);
static soap_wchar soap_get_pi(struct soap*);
static int soap_isxdigit(int);
static void *fplugin(struct soap*, const char*);
static size_t soap_count_attachments(struct soap *soap);
static int soap_try_connect_command(struct soap*, int http_command, const char *endpoint, const char *action);
#ifdef WITH_NTLM
static int soap_ntlm_handshake(struct soap *soap, int command, const char *endpoint, const char *host, int port);
#endif
#ifndef WITH_NOIDREF
static int soap_has_copies(struct soap*, const char*, const char*);
static int soap_type_punned(struct soap*, const struct soap_ilist*);
static int soap_is_shaky(struct soap*, void*);
#endif
static void soap_init_iht(struct soap*);
static void soap_free_iht(struct soap*);
static void soap_init_pht(struct soap*);
static void soap_free_pht(struct soap*);
#endif

#ifndef WITH_LEAN
static const char *soap_set_validation_fault(struct soap*, const char*, const char*);
static int soap_isnumeric(struct soap*, const char*);
static struct soap_nlist *soap_push_ns(struct soap *soap, const char *id, const char *ns, short utilized);
static void soap_utilize_ns(struct soap *soap, const char *tag);
static const wchar_t* soap_wstring(struct soap *soap, const char *s, long minlen, long maxlen);
#endif

#ifndef PALM_2
static const char* soap_string(struct soap *soap, const char *s, long minlen, long maxlen);
static const char* soap_QName(struct soap *soap, const char *s, long minlen, long maxlen);
#endif

#ifndef WITH_LEANER
#ifndef PALM_1
static struct soap_multipart *soap_new_multipart(struct soap*, struct soap_multipart**, struct soap_multipart**, char*, size_t);
static int soap_putdimefield(struct soap*, const char*, size_t);
static char *soap_getdimefield(struct soap*, size_t);
static void soap_select_mime_boundary(struct soap*);
static int soap_valid_mime_boundary(struct soap*);
static void soap_resolve_attachment(struct soap*, struct soap_multipart*);
#endif
#endif

#ifdef WITH_GZIP
static int soap_getgziphdr(struct soap*);
#endif

#ifdef WITH_OPENSSL
# ifndef SOAP_SSL_RSA_BITS
#  define SOAP_SSL_RSA_BITS 2048
# endif
static int soap_ssl_init_done = 0;
static int ssl_auth_init(struct soap*);
static int ssl_verify_callback(int, X509_STORE_CTX*);
static int ssl_verify_callback_allow_expired_certificate(int, X509_STORE_CTX*);
static int ssl_password(char*, int, int, void *);
#endif

#ifdef WITH_GNUTLS
# ifndef SOAP_SSL_RSA_BITS
#  define SOAP_SSL_RSA_BITS 2048
# endif
static int soap_ssl_init_done = 0;
static int ssl_auth_init(struct soap*);
static const char *ssl_verify(struct soap *soap, const char *host);
# if defined(HAVE_PTHREAD_H)
#  include <pthread.h>
   /* make GNUTLS thread safe with pthreads */
   GCRY_THREAD_OPTION_PTHREAD_IMPL;
# elif defined(HAVE_PTH_H)
   #include <pth.h>
   /* make GNUTLS thread safe with PTH */
   GCRY_THREAD_OPTION_PTH_IMPL;
# endif
#endif

#ifdef WITH_SYSTEMSSL
static int ssl_auth_init(struct soap*);
static int ssl_recv(int sk, void *s, int n, char *user);
static int ssl_send(int sk, void *s, int n, char *user);
#endif

#if !defined(WITH_NOHTTP) || !defined(WITH_LEANER)
#ifndef PALM_1
static const char *soap_decode(char*, size_t, const char*, const char*);
#endif
#endif

#ifndef WITH_NOHTTP
#ifndef PALM_1
static soap_wchar soap_getchunkchar(struct soap*);
static const char *http_error(struct soap*, int);
static int http_get(struct soap*);
static int http_405(struct soap*);
static int http_200(struct soap*);
static int http_post(struct soap*, const char*, const char*, int, const char*, const char*, size_t);
static int http_send_header(struct soap*, const char*);
static int http_post_header(struct soap*, const char*, const char*);
static int http_response(struct soap*, int, size_t);
static int http_parse(struct soap*);
static int http_parse_header(struct soap*, const char*, const char*);
#endif
#endif

#ifndef WITH_NOIO

#ifndef PALM_1
static int fsend(struct soap*, const char*, size_t);
static size_t frecv(struct soap*, char*, size_t);
static int tcp_init(struct soap*);
static const char *tcp_error(struct soap*);
#ifndef WITH_IPV6
static int tcp_gethost(struct soap*, const char *addr, struct in_addr *inaddr);
#endif
static SOAP_SOCKET tcp_connect(struct soap*, const char *endpoint, const char *host, int port);
static SOAP_SOCKET tcp_accept(struct soap*, SOAP_SOCKET, struct sockaddr*, int*);
static int tcp_select(struct soap*, SOAP_SOCKET, int, int);
static int tcp_disconnect(struct soap*);
static int tcp_closesocket(struct soap*, SOAP_SOCKET);
static int tcp_shutdownsocket(struct soap*, SOAP_SOCKET, int);
static const char *soap_strerror(struct soap*);
#endif

#define SOAP_TCP_SELECT_RCV 0x1
#define SOAP_TCP_SELECT_SND 0x2
#define SOAP_TCP_SELECT_ERR 0x4
#define SOAP_TCP_SELECT_ALL 0x7

#if defined(WIN32)
  #define SOAP_SOCKBLOCK(fd) \
  { u_long blocking = 0; \
    ioctlsocket(fd, FIONBIO, &blocking); \
  }
  #define SOAP_SOCKNONBLOCK(fd) \
  { u_long nonblocking = 1; \
    ioctlsocket(fd, FIONBIO, &nonblocking); \
  }
#elif defined(VXWORKS)
  #define SOAP_SOCKBLOCK(fd) \
  { u_long blocking = 0; \
    ioctl(fd, FIONBIO, (int)(&blocking)); \
  }
  #define SOAP_SOCKNONBLOCK(fd) \
  { u_long nonblocking = 1; \
    ioctl(fd, FIONBIO, (int)(&nonblocking)); \
  }
#elif defined(__VMS)
  #define SOAP_SOCKBLOCK(fd) \
  { int blocking = 0; \
    ioctl(fd, FIONBIO, &blocking); \
  }
  #define SOAP_SOCKNONBLOCK(fd) \
  { int nonblocking = 1; \
    ioctl(fd, FIONBIO, &nonblocking); \
  }
#elif defined(PALM)
  #define SOAP_SOCKBLOCK(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)&~O_NONBLOCK);
  #define SOAP_SOCKNONBLOCK(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)|O_NONBLOCK);
#elif defined(SYMBIAN)
  #define SOAP_SOCKBLOCK(fd) \
  { long blocking = 0; \
    ioctl(fd, 0/*FIONBIO*/, &blocking); \
  }
  #define SOAP_SOCKNONBLOCK(fd) \
  { long nonblocking = 1; \
    ioctl(fd, 0/*FIONBIO*/, &nonblocking); \
  }
#else
  #define SOAP_SOCKBLOCK(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)&~O_NONBLOCK);
  #define SOAP_SOCKNONBLOCK(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)|O_NONBLOCK);
#endif

#endif

#if defined(PALM) && !defined(PALM_2)
unsigned short errno;
#endif

#ifndef PALM_1
static const char soap_env1[42] = "http://schemas.xmlsoap.org/soap/envelope/";
static const char soap_enc1[42] = "http://schemas.xmlsoap.org/soap/encoding/";
static const char soap_env2[40] = "http://www.w3.org/2003/05/soap-envelope";
static const char soap_enc2[40] = "http://www.w3.org/2003/05/soap-encoding";
static const char soap_rpc[35] = "http://www.w3.org/2003/05/soap-rpc";
#endif

#ifndef PALM_1
const union soap_double_nan soap_double_nan = {{0xFFFFFFFF, 0xFFFFFFFF}};
const char soap_base64o[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char soap_base64i[81] = "\76XXX\77\64\65\66\67\70\71\72\73\74\75XXXXXXX\00\01\02\03\04\05\06\07\10\11\12\13\14\15\16\17\20\21\22\23\24\25\26\27\30\31XXXXXX\32\33\34\35\36\37\40\41\42\43\44\45\46\47\50\51\52\53\54\55\56\57\60\61\62\63";
#endif

#ifndef WITH_LEAN
static const char soap_indent[11] = "\n\t\t\t\t\t\t\t\t\t";
/* Alternative indentation form for SOAP_XML_INDENT:
static const char soap_indent[21] = "\n                   ";
*/
#endif

#ifndef SOAP_CANARY
# define SOAP_CANARY (0xC0DE)
#endif

static const char soap_padding[4] = "\0\0\0";
#define SOAP_STR_PADDING (soap_padding)
#define SOAP_STR_EOS (soap_padding)
#define SOAP_NON_NULL (soap_padding)

#ifndef WITH_LEAN
static const struct soap_code_map html_entity_codes[] = /* entities for XHTML parsing */
{ { 160, "nbsp" },
  { 161, "iexcl" },
  { 162, "cent" },
  { 163, "pound" },
  { 164, "curren" },
  { 165, "yen" },
  { 166, "brvbar" },
  { 167, "sect" },
  { 168, "uml" },
  { 169, "copy" },
  { 170, "ordf" },
  { 171, "laquo" },
  { 172, "not" },
  { 173, "shy" },
  { 174, "reg" },
  { 175, "macr" },
  { 176, "deg" },
  { 177, "plusmn" },
  { 178, "sup2" },
  { 179, "sup3" },
  { 180, "acute" },
  { 181, "micro" },
  { 182, "para" },
  { 183, "middot" },
  { 184, "cedil" },
  { 185, "sup1" },
  { 186, "ordm" },
  { 187, "raquo" },
  { 188, "frac14" },
  { 189, "frac12" },
  { 190, "frac34" },
  { 191, "iquest" },
  { 192, "Agrave" },
  { 193, "Aacute" },
  { 194, "Acirc" },
  { 195, "Atilde" },
  { 196, "Auml" },
  { 197, "Aring" },
  { 198, "AElig" },
  { 199, "Ccedil" },
  { 200, "Egrave" },
  { 201, "Eacute" },
  { 202, "Ecirc" },
  { 203, "Euml" },
  { 204, "Igrave" },
  { 205, "Iacute" },
  { 206, "Icirc" },
  { 207, "Iuml" },
  { 208, "ETH" },
  { 209, "Ntilde" },
  { 210, "Ograve" },
  { 211, "Oacute" },
  { 212, "Ocirc" },
  { 213, "Otilde" },
  { 214, "Ouml" },
  { 215, "times" },
  { 216, "Oslash" },
  { 217, "Ugrave" },
  { 218, "Uacute" },
  { 219, "Ucirc" },
  { 220, "Uuml" },
  { 221, "Yacute" },
  { 222, "THORN" },
  { 223, "szlig" },
  { 224, "agrave" },
  { 225, "aacute" },
  { 226, "acirc" },
  { 227, "atilde" },
  { 228, "auml" },
  { 229, "aring" },
  { 230, "aelig" },
  { 231, "ccedil" },
  { 232, "egrave" },
  { 233, "eacute" },
  { 234, "ecirc" },
  { 235, "euml" },
  { 236, "igrave" },
  { 237, "iacute" },
  { 238, "icirc" },
  { 239, "iuml" },
  { 240, "eth" },
  { 241, "ntilde" },
  { 242, "ograve" },
  { 243, "oacute" },
  { 244, "ocirc" },
  { 245, "otilde" },
  { 246, "ouml" },
  { 247, "divide" },
  { 248, "oslash" },
  { 249, "ugrave" },
  { 250, "uacute" },
  { 251, "ucirc" },
  { 252, "uuml" },
  { 253, "yacute" },
  { 254, "thorn" },
  { 255, "yuml" },
  {   0, NULL }
};
#endif

#ifndef WITH_NOIO
#ifndef WITH_LEAN
static const struct soap_code_map h_error_codes[] =
{
#ifdef HOST_NOT_FOUND
  { HOST_NOT_FOUND, "Host not found" },
#endif
#ifdef TRY_AGAIN
  { TRY_AGAIN, "Try Again" },
#endif
#ifdef NO_RECOVERY
  { NO_RECOVERY, "No Recovery" },
#endif
#ifdef NO_DATA
  { NO_DATA, "No Data" },
#endif
#ifdef NO_ADDRESS
  { NO_ADDRESS, "No Address" },
#endif
  { 0, NULL }
};
#endif
#endif

#ifndef WITH_NOHTTP
#ifndef WITH_LEAN
static const struct soap_code_map h_http_error_codes[] =
{ { 200, "OK" },
  { 201, "Created" },
  { 202, "Accepted" },
  { 203, "Non-Authoritative Information" },
  { 204, "No Content" },
  { 205, "Reset Content" },
  { 206, "Partial Content" },
  { 300, "Multiple Choices" },
  { 301, "Moved Permanently" },
  { 302, "Found" },
  { 303, "See Other" },
  { 304, "Not Modified" },
  { 305, "Use Proxy" },
  { 307, "Temporary Redirect" },
  { 400, "Bad Request" },
  { 401, "Unauthorized" },
  { 402, "Payment Required" },
  { 403, "Forbidden" },
  { 404, "Not Found" },
  { 405, "Method Not Allowed" },
  { 406, "Not Acceptable" },
  { 407, "Proxy Authentication Required" },
  { 408, "Request Time-out" },
  { 409, "Conflict" },
  { 410, "Gone" },
  { 411, "Length Required" },
  { 412, "Precondition Failed" },
  { 413, "Request Entity Too Large" },
  { 414, "Request-URI Too Large" },
  { 415, "Unsupported Media Type" },
  { 416, "Requested range not satisfiable" },
  { 417, "Expectation Failed" },
  { 500, "Internal Server Error" },
  { 501, "Not Implemented" },
  { 502, "Bad Gateway" },
  { 503, "Service Unavailable" },
  { 504, "Gateway Time-out" },
  { 505, "HTTP Version not supported" },
  {   0, NULL }
};
#endif
#endif

#ifdef WITH_OPENSSL
static const struct soap_code_map h_ssl_error_codes[] =
{
#define _SSL_ERROR(e) { e, #e }
  _SSL_ERROR(SSL_ERROR_SSL),
  _SSL_ERROR(SSL_ERROR_ZERO_RETURN),
  _SSL_ERROR(SSL_ERROR_WANT_READ),
  _SSL_ERROR(SSL_ERROR_WANT_WRITE),
  _SSL_ERROR(SSL_ERROR_WANT_CONNECT),
  _SSL_ERROR(SSL_ERROR_WANT_X509_LOOKUP),
  _SSL_ERROR(SSL_ERROR_SYSCALL),
  { 0, NULL }
};
#endif

#ifndef WITH_LEANER
static const struct soap_code_map mime_codes[] =
{ { SOAP_MIME_7BIT,		"7bit" },
  { SOAP_MIME_8BIT,		"8bit" },
  { SOAP_MIME_BINARY,		"binary" },
  { SOAP_MIME_QUOTED_PRINTABLE, "quoted-printable" },
  { SOAP_MIME_BASE64,		"base64" },
  { SOAP_MIME_IETF_TOKEN,	"ietf-token" },
  { SOAP_MIME_X_TOKEN,		"x-token" },
  { 0,				NULL }
};
#endif

#ifdef WIN32
static int tcp_done = 0;
#endif

#if defined(HP_UX) && defined(HAVE_GETHOSTBYNAME_R)
extern int h_errno;
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static int
fsend(struct soap *soap, const char *s, size_t n)
{ int nwritten, err;
  SOAP_SOCKET sk;
#if defined(__cplusplus) && !defined(WITH_LEAN) && !defined(WITH_COMPAT)
  if (soap->os)
  { soap->os->write(s, (std::streamsize)n);
    if (soap->os->good())
      return SOAP_OK;
    soap->errnum = 0;
    return SOAP_EOF;
  }
#endif
  sk = soap->sendsk;
  if (!soap_valid_socket(sk))
    sk = soap->socket;
  while (n)
  { if (soap_valid_socket(sk))
    {
      if (soap->send_timeout)
      { for (;;)
        { int r;
#ifdef WITH_OPENSSL
          if (soap->ssl)
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_ALL, soap->send_timeout);
          else
#endif
#ifdef WITH_GNUTLS
          if (soap->session)
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_ALL, soap->send_timeout);
          else
#endif
#ifdef WITH_SYSTEMSSL
          if (soap->ssl)
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_ALL, soap->send_timeout);
          else
#endif
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, soap->send_timeout);
          if (r > 0)
            break;
          if (!r)
            return SOAP_EOF;
          err = soap->errnum;
          if (!err)
            return soap->error;
          if (err != SOAP_EAGAIN && err != SOAP_EWOULDBLOCK)
            return SOAP_EOF;
        }
      }
#ifdef WITH_OPENSSL
      if (soap->ssl)
        nwritten = SSL_write(soap->ssl, s, (int)n);
      else if (soap->bio)
        nwritten = BIO_write(soap->bio, s, (int)n);
      else
#endif
#ifdef WITH_GNUTLS
      if (soap->session)
        nwritten = gnutls_record_send(soap->session, s, n);
      else
#endif
#ifdef WITH_SYSTEMSSL
      if (soap->ssl)
      { err = gsk_secure_socket_write(soap->ssl, (char*)s, n, &nwritten);
	if (err != GSK_OK)
	  nwritten = 0;
      }
      else
#endif
#ifndef WITH_LEAN
      if ((soap->omode & SOAP_IO_UDP))
      { if (soap->peerlen)
          nwritten = sendto(sk, (char*)s, (SOAP_WINSOCKINT)n, soap->socket_flags, &soap->peer.addr, (SOAP_WINSOCKINT)soap->peerlen);
        else
          nwritten = send(sk, s, (SOAP_WINSOCKINT)n, soap->socket_flags);
        /* retry and back-off algorithm */
        /* TODO: this is not very clear from specs so verify and limit conditions under which we should loop (e.g. ENOBUFS) */
        if (nwritten < 0)
        { int udp_repeat;
          int udp_delay;
          if ((soap->connect_flags & SO_BROADCAST))
            udp_repeat = 2; /* SOAP-over-UDP MULTICAST_UDP_REPEAT - 1 */
          else
            udp_repeat = 1; /* SOAP-over-UDP UNICAST_UDP_REPEAT - 1 */
          udp_delay = ((unsigned int)soap_random % 201) + 50; /* UDP_MIN_DELAY .. UDP_MAX_DELAY */
          do
          { tcp_select(soap, sk, SOAP_TCP_SELECT_ERR, -1000 * udp_delay);
            if (soap->peerlen)
              nwritten = sendto(sk, (char*)s, (SOAP_WINSOCKINT)n, soap->socket_flags, &soap->peer.addr, (SOAP_WINSOCKINT)soap->peerlen);
            else
              nwritten = send(sk, s, (SOAP_WINSOCKINT)n, soap->socket_flags);
            udp_delay <<= 1;
            if (udp_delay > 500) /* UDP_UPPER_DELAY */
              udp_delay = 500;
          } while (nwritten < 0 && --udp_repeat > 0);
        }
        if (nwritten < 0)
        { err = soap_socket_errno(sk);
          if (err && err != SOAP_EINTR)
          { soap->errnum = err;
            return SOAP_EOF;
          }
          nwritten = 0; /* and call write() again */
        }
      }
      else
#endif
#if !defined(PALM) && !defined(AS400)
        nwritten = send(sk, s, (int)n, soap->socket_flags);
#else
        nwritten = send(sk, (void*)s, n, soap->socket_flags);
#endif
      if (nwritten <= 0)
      {
        int r = 0;
        err = soap_socket_errno(sk);
#ifdef WITH_OPENSSL
        if (soap->ssl && (r = SSL_get_error(soap->ssl, nwritten)) != SSL_ERROR_NONE && r != SSL_ERROR_WANT_READ && r != SSL_ERROR_WANT_WRITE)
        { soap->errnum = err;
          return SOAP_EOF;
        }
#endif
#ifdef WITH_GNUTLS
        if (soap->session)
        { if (nwritten == GNUTLS_E_INTERRUPTED)
            err = SOAP_EINTR;
          else if (nwritten == GNUTLS_E_AGAIN)
            err = SOAP_EAGAIN;
        }
#endif
        if (err == SOAP_EWOULDBLOCK || err == SOAP_EAGAIN)
        {
#if defined(WITH_OPENSSL)
          if (soap->ssl && r == SSL_ERROR_WANT_READ)
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, soap->send_timeout ? soap->send_timeout : -10000);
          else
#elif defined(WITH_GNUTLS)
          if (soap->session && !gnutls_record_get_direction(soap->session))
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, soap->send_timeout ? soap->send_timeout : -10000);
          else
#endif
            r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, soap->send_timeout ? soap->send_timeout : -10000);
          if (!r && soap->send_timeout)
            return SOAP_EOF;
          if (r < 0)
            return SOAP_EOF;
        }
        else if (err && err != SOAP_EINTR)
        { soap->errnum = err;
          return SOAP_EOF;
        }
        nwritten = 0; /* and call write() again */
      }
    }
    else
    {
#ifdef WITH_FASTCGI
      nwritten = fwrite((void*)s, 1, n, stdout);
      fflush(stdout);
#else
#ifdef UNDER_CE
      nwritten = fwrite(s, 1, n, soap->sendfd);
#else
#ifdef VXWORKS
#ifdef WMW_RPM_IO
      if (soap->rpmreqid)
        nwritten = (httpBlockPut(soap->rpmreqid, (char*)s, n) == 0) ? n : -1;
      else
#endif
        nwritten = fwrite(s, sizeof(char), n, fdopen(soap->sendfd, "w"));
#else
#ifdef WIN32
      nwritten = _write(soap->sendfd, s, (unsigned int)n);
#else
      nwritten = write(soap->sendfd, s, (unsigned int)n);
#endif
#endif
#endif
#endif
      if (nwritten <= 0)
      {
#ifndef WITH_FASTCGI
        err = soap_errno;
#else
        err = EOF;
#endif
        if (err && err != SOAP_EINTR && err != SOAP_EWOULDBLOCK && err != SOAP_EAGAIN)
        { soap->errnum = err;
          return SOAP_EOF;
        }
        nwritten = 0; /* and call write() again */
      }
    }
    n -= nwritten;
    s += nwritten;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_send_raw(struct soap *soap, const char *s, size_t n)
{ if (!n)
    return SOAP_OK;
#ifndef WITH_LEANER
  if (soap->fpreparesend && (soap->mode & SOAP_IO) != SOAP_IO_STORE && (soap->mode & SOAP_IO_LENGTH) && (soap->error = soap->fpreparesend(soap, s, n)))
    return soap->error;
  if (soap->ffiltersend && (soap->error = soap->ffiltersend(soap, &s, &n)))
    return soap->error;
#endif
  if (soap->mode & SOAP_IO_LENGTH)
    soap->count += n;
  else if (soap->mode & SOAP_IO)
  { size_t i = sizeof(soap->buf) - soap->bufidx;
    while (n >= i)
    { soap_memcpy((void*)(soap->buf + soap->bufidx), i, (const void*)s, i);
      soap->bufidx = sizeof(soap->buf);
      if (soap_flush(soap))
        return soap->error;
      s += i;
      n -= i;
      i = sizeof(soap->buf);
    }
    soap_memcpy((void*)(soap->buf + soap->bufidx), sizeof(soap->buf) - soap->bufidx, (const void*)s, n);
    soap->bufidx += n;
  }
  else
    return soap_flush_raw(soap, s, n);
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_flush(struct soap *soap)
{ size_t n = soap->bufidx;
  if (n)
  {
#ifndef WITH_LEANER
    if ((soap->mode & SOAP_IO) == SOAP_IO_STORE)
    { int r;
      if (soap->fpreparesend && (r = soap->fpreparesend(soap, soap->buf, n)))
        return soap->error = r;
    }
#endif
    soap->bufidx = 0;
#ifdef WITH_ZLIB
    if (soap->mode & SOAP_ENC_ZLIB)
    { soap->d_stream->next_in = (Byte*)soap->buf;
      soap->d_stream->avail_in = (unsigned int)n;
#ifdef WITH_GZIP
      soap->z_crc = crc32(soap->z_crc, (Byte*)soap->buf, (unsigned int)n);
#endif
      do
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Deflating %u bytes\n", soap->d_stream->avail_in));
        if (deflate(soap->d_stream, Z_NO_FLUSH) != Z_OK)
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Unable to deflate: %s\n", soap->d_stream->msg ? soap->d_stream->msg : SOAP_STR_EOS));
          return soap->error = SOAP_ZLIB_ERROR;
        }
        if (!soap->d_stream->avail_out)
        { if (soap_flush_raw(soap, soap->z_buf, sizeof(soap->buf)))
            return soap->error;
          soap->d_stream->next_out = (Byte*)soap->z_buf;
          soap->d_stream->avail_out = sizeof(soap->buf);
        }
      } while (soap->d_stream->avail_in);
    }
    else
#endif
      return soap_flush_raw(soap, soap->buf, n);
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_flush_raw(struct soap *soap, const char *s, size_t n)
{ if ((soap->mode & SOAP_IO) == SOAP_IO_STORE)
  { void *t;
    if (!(t = soap_push_block(soap, NULL, n)))
      return soap->error = SOAP_EOM;
    soap_memcpy(t, n, (const void*)s, n);
    return SOAP_OK;
  }
#ifndef WITH_LEANER
  if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK)
  { char t[24];
    (SOAP_SNPRINTF(t, sizeof(t), 20), &"\r\n%lX\r\n"[soap->chunksize ? 0 : 2], (unsigned long)n);
    DBGMSG(SENT, t, strlen(t));
    if ((soap->error = soap->fsend(soap, t, strlen(t))))
      return soap->error;
    soap->chunksize += n;
  }
  DBGMSG(SENT, s, n);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Send %u bytes to socket=%d/fd=%d\n", (unsigned int)n, soap->socket, soap->sendfd));
#endif
  return soap->error = soap->fsend(soap, s, n);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_send(struct soap *soap, const char *s)
{ if (s)
    return soap_send_raw(soap, s, strlen(s));
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_send2(struct soap *soap, const char *s1, const char *s2)
{ if (soap_send(soap, s1))
    return soap->error;
  return soap_send(soap, s2);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_send3(struct soap *soap, const char *s1, const char *s2, const char *s3)
{ if (soap_send(soap, s1)
   || soap_send(soap, s2))
    return soap->error;
  return soap_send(soap, s3);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static size_t
frecv(struct soap *soap, char *s, size_t n)
{ int r;
  int retries = 100; /* max 100 retries with non-blocking sockets */
  SOAP_SOCKET sk;
  soap->errnum = 0;
#if defined(__cplusplus) && !defined(WITH_LEAN) && !defined(WITH_COMPAT)
  if (soap->is)
  { if (soap->is->good())
      return (size_t)soap->is->read(s, (std::streamsize)n).gcount();
    return 0;
  }
#endif
  sk = soap->recvsk;
  if (!soap_valid_socket(sk))
    sk = soap->socket;
  if (soap_valid_socket(sk))
  { for (;;)
    {
#if defined(WITH_OPENSSL) || defined(WITH_SYSTEMSSL)
      int err = 0;
#endif
#ifdef WITH_OPENSSL
      if (soap->recv_timeout && !soap->ssl) /* OpenSSL: sockets are nonblocking */
#else
      if (soap->recv_timeout)
#endif
      { for (;;)
        { r = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, soap->recv_timeout);
          if (r > 0)
            break;
          if (!r)
            return 0;
          r = soap->errnum;
          if (r != SOAP_EAGAIN && r != SOAP_EWOULDBLOCK)
            return 0;
        }
      }
#ifdef WITH_OPENSSL
      if (soap->ssl)
      { r = SSL_read(soap->ssl, s, (int)n);
        if (r > 0)
          return (size_t)r;
        err = SSL_get_error(soap->ssl, r);
        if (err != SSL_ERROR_NONE && err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
          return 0;
      }
      else if (soap->bio)
      { r = BIO_read(soap->bio, s, (int)n);
        if (r > 0)
          return (size_t)r;
        return 0;
      }
      else
#endif
#ifdef WITH_GNUTLS
      if (soap->session)
      { r = (int)gnutls_record_recv(soap->session, s, n);
        if (r >= 0)
          return (size_t)r;
      }
      else
#endif
#ifdef WITH_SYSTEMSSL
      if (soap->ssl)
      { err = gsk_secure_socket_read(soap->ssl, s, n, &r);
        if (err == GSK_OK && r > 0)
	  return (size_t)r;
        if (err != GSK_OK && err != GSK_WOULD_BLOCK && err != GSK_WOULD_BLOCK_WRITE)
	  return 0;
      }
      else
#endif
      {
#ifndef WITH_LEAN
        if ((soap->omode & SOAP_IO_UDP))
        { SOAP_SOCKLEN_T k = (SOAP_SOCKLEN_T)sizeof(soap->peer);
          memset((void*)&soap->peer, 0, sizeof(soap->peer));
          r = recvfrom(sk, s, (SOAP_WINSOCKINT)n, soap->socket_flags, &soap->peer.addr, &k);	/* portability note: see SOAP_SOCKLEN_T definition in stdsoap2.h */
          soap->peerlen = (size_t)k;
#ifndef WITH_IPV6
          soap->ip = ntohl(soap->peer.in.sin_addr.s_addr);
#endif
        }
        else
#endif
          r = recv(sk, s, (SOAP_WINSOCKINT)n, soap->socket_flags);
#ifdef PALM
        /* CycleSyncDisplay(curStatusMsg); */
#endif
        if (r >= 0)
          return (size_t)r;
        r = soap_socket_errno(sk);
        if (r != SOAP_EINTR && r != SOAP_EAGAIN && r != SOAP_EWOULDBLOCK)
        { soap->errnum = r;
          return 0;
        }
      }
#if defined(WITH_OPENSSL)
      if (soap->ssl && err == SSL_ERROR_WANT_WRITE)
        r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, soap->recv_timeout ? soap->recv_timeout : 5);
      else
#elif defined(WITH_GNUTLS)
      if (soap->session && gnutls_record_get_direction(soap->session))
        r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, soap->recv_timeout ? soap->recv_timeout : 5);
      else
#elif defined(WITH_SYSTEMSSL)
      if (soap->ssl && err == GSK_WOULD_BLOCK_WRITE)
        r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, soap->recv_timeout ? soap->recv_timeout : 5);
      else
#endif
        r = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, soap->recv_timeout ? soap->recv_timeout : 5);
      if (!r && soap->recv_timeout)
        return 0;
      if (r < 0)
      { r = soap->errnum;
        if (r != SOAP_EAGAIN && r != SOAP_EWOULDBLOCK)
          return 0;
      }
      if (retries-- <= 0)
        return 0;
#ifdef PALM
      r = soap_socket_errno(sk);
      if (r != SOAP_EINTR && retries-- <= 0)
      { soap->errnum = r;
        return 0;
      }
#endif
    }
  }
#ifdef WITH_FASTCGI
  return fread(s, 1, n, stdin);
#else
#ifdef UNDER_CE
  return fread(s, 1, n, soap->recvfd);
#else
#ifdef WMW_RPM_IO
  if (soap->rpmreqid)
    r = httpBlockRead(soap->rpmreqid, s, n);
  else
#endif
#ifdef WIN32
    r = _read(soap->recvfd, s, (unsigned int)n);
#else
    r = read(soap->recvfd, s, n);
#endif
  if (r >= 0)
    return (size_t)r;
  soap->errnum = soap_errno;
  return 0;
#endif
#endif
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static soap_wchar
soap_getchunkchar(struct soap *soap)
{ if (soap->bufidx < soap->buflen)
    return soap->buf[soap->bufidx++];
  soap->bufidx = 0;
  soap->buflen = soap->chunkbuflen = soap->frecv(soap, soap->buf, sizeof(soap->buf));
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Read %u bytes from socket=%d/fd=%d\n", (unsigned int)soap->buflen, soap->socket, soap->recvfd));
  DBGMSG(RECV, soap->buf, soap->buflen);
  if (soap->buflen)
    return soap->buf[soap->bufidx++];
  return EOF;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
static int
soap_isxdigit(int c)
{ return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_recv_raw(struct soap *soap)
{ size_t ret;
#if !defined(WITH_LEANER) || defined(WITH_ZLIB)
  int r;
#endif
#ifdef WITH_ZLIB
  if ((soap->mode & SOAP_ENC_ZLIB) && soap->d_stream)
  { if (soap->d_stream->next_out == Z_NULL)
    { soap->bufidx = soap->buflen = 0;
      return EOF;
    }
    if (soap->d_stream->avail_in || !soap->d_stream->avail_out)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflating\n"));
      soap->d_stream->next_out = (Byte*)soap->buf;
      soap->d_stream->avail_out = sizeof(soap->buf);
      r = inflate(soap->d_stream, Z_NO_FLUSH);
      if (r == Z_NEED_DICT && soap->z_dict)
        r = inflateSetDictionary(soap->d_stream, (const Bytef*)soap->z_dict, soap->z_dict_len);
      if (r == Z_OK || r == Z_STREAM_END)
      { soap->bufidx = 0;
        ret = soap->buflen = sizeof(soap->buf) - soap->d_stream->avail_out;
        if (soap->zlib_in == SOAP_ZLIB_GZIP)
          soap->z_crc = crc32(soap->z_crc, (Byte*)soap->buf, (unsigned int)ret);
        if (r == Z_STREAM_END)
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflated %lu->%lu bytes\n", soap->d_stream->total_in, soap->d_stream->total_out));
          soap->z_ratio_in = (float)soap->d_stream->total_in / (float)soap->d_stream->total_out;
          soap->d_stream->next_out = Z_NULL;
        }
        if (ret)
        { soap->count += ret;
          if (soap->count > SOAP_MAXINFLATESIZE && soap->z_ratio_in < SOAP_MINDEFLATERATIO)
          { soap->d_stream->msg = (char*)"caught SOAP_MINDEFLATERATIO explosive decompression guard (remedy: increase SOAP_MAXINFLATESIZE and/or decrease SOAP_MINDEFLATERATIO)";
            return soap->error = SOAP_ZLIB_ERROR;
          }
          DBGLOG(RECV, SOAP_MESSAGE(fdebug, "\n---- decompressed ----\n"));
          DBGMSG(RECV, soap->buf, ret);
          DBGLOG(RECV, SOAP_MESSAGE(fdebug, "\n----\n"));
#ifndef WITH_LEANER
          if (soap->fpreparerecv && (r = soap->fpreparerecv(soap, soap->buf, ret)))
            return soap->error = r;
#endif
          return SOAP_OK;
        }
      }
      else if (r != Z_BUF_ERROR)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflate error: %s\n", soap->d_stream->msg ? soap->d_stream->msg : SOAP_STR_EOS));
        soap->d_stream->next_out = Z_NULL;
        return soap->error = SOAP_ZLIB_ERROR;
      }
    }
zlib_again:
    if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK && !soap->chunksize)
    { soap_memcpy((void*)soap->buf, sizeof(soap->buf), (const void*)soap->z_buf, sizeof(soap->buf));
      soap->buflen = soap->z_buflen;
    }
    DBGLOG(RECV, SOAP_MESSAGE(fdebug, "\n---- compressed ----\n"));
  }
#endif
#ifndef WITH_NOHTTP
  if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK) /* read HTTP chunked transfer */
  { for (;;)
    { soap_wchar c;
      char *t, tmp[17];
      if (soap->chunksize)
      { soap->buflen = ret = soap->frecv(soap, soap->buf, soap->chunksize > sizeof(soap->buf) ? sizeof(soap->buf) : soap->chunksize);
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Getting chunk: read %u bytes\n", (unsigned int)ret));
        DBGMSG(RECV, soap->buf, ret);
        soap->bufidx = 0;
        soap->chunksize -= ret;
        break;
      }
      t = tmp;
      if (!soap->chunkbuflen)
      { soap->chunkbuflen = ret = soap->frecv(soap, soap->buf, sizeof(soap->buf));
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Read %u bytes (chunked) from socket=%d\n", (unsigned int)ret, soap->socket));
        DBGMSG(RECV, soap->buf, ret);
        soap->bufidx = 0;
        if (!ret)
        { soap->ahead = EOF;
          return EOF;
        }
      }
      else
        soap->bufidx = soap->buflen;
      soap->buflen = soap->chunkbuflen;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Getting chunk size (idx=%u len=%u)\n", (unsigned int)soap->bufidx, (unsigned int)soap->buflen));
      while (!soap_isxdigit((int)(c = soap_getchunkchar(soap))))
      { if ((int)c == EOF)
        { soap->ahead = EOF;
          return EOF;
        }
      }
      do
        *t++ = (char)c;
      while (soap_isxdigit((int)(c = soap_getchunkchar(soap))) && (size_t)(t - tmp) < sizeof(tmp)-1);
      while ((int)c != EOF && c != '\n')
        c = soap_getchunkchar(soap);
      if ((int)c == EOF)
      { soap->ahead = EOF;
        return EOF;
      }
      *t = '\0';
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Chunk size = %s (hex)\n", tmp));
      soap->chunksize = (size_t)soap_strtoul(tmp, &t, 16);
      if (!soap->chunksize)
      { soap->bufidx = soap->buflen = soap->chunkbuflen = 0;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End of chunked message\n"));
        while ((int)c != EOF && c != '\n')
          c = soap_getchunkchar(soap);
        ret = 0;
        soap->ahead = EOF;
        break;
      }
      soap->buflen = soap->bufidx + soap->chunksize;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Moving buf len to idx=%u len=%u (%s)\n", (unsigned int)soap->bufidx, (unsigned int)soap->buflen, tmp));
      if (soap->buflen > soap->chunkbuflen)
      { soap->buflen = soap->chunkbuflen;
        soap->chunksize -= soap->buflen - soap->bufidx;
        soap->chunkbuflen = 0;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Passed end of buffer for chunked HTTP (%u bytes left)\n", (unsigned int)(soap->buflen - soap->bufidx)));
      }
      else if (soap->chunkbuflen)
        soap->chunksize = 0;
      ret = soap->buflen - soap->bufidx;
      if (ret)
        break;
    }
  }
  else
#endif
  { soap->bufidx = 0;
    soap->buflen = ret = soap->frecv(soap, soap->buf, sizeof(soap->buf));
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Read %u bytes from socket=%d/fd=%d\n", (unsigned int)ret, soap->socket, soap->recvfd));
    DBGMSG(RECV, soap->buf, ret);
  }
#ifdef WITH_ZLIB
  if (soap->mode & SOAP_ENC_ZLIB)
  { soap_memcpy((void*)soap->z_buf, sizeof(soap->buf), (const void*)soap->buf, sizeof(soap->buf));
    soap->d_stream->next_in = (Byte*)(soap->z_buf + soap->bufidx);
    soap->d_stream->avail_in = (unsigned int)ret;
    soap->d_stream->next_out = (Byte*)soap->buf;
    soap->d_stream->avail_out = sizeof(soap->buf);
    r = inflate(soap->d_stream, Z_NO_FLUSH);
    if (r == Z_NEED_DICT && soap->z_dict)
      r = inflateSetDictionary(soap->d_stream, (const Bytef*)soap->z_dict, soap->z_dict_len);
    if (r == Z_OK || r == Z_STREAM_END)
    { soap->bufidx = 0;
      soap->z_buflen = soap->buflen;
      soap->buflen = sizeof(soap->buf) - soap->d_stream->avail_out;
      if (soap->zlib_in == SOAP_ZLIB_GZIP)
        soap->z_crc = crc32(soap->z_crc, (Byte*)soap->buf, (unsigned int)soap->buflen);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflated %u bytes\n", (unsigned int)soap->buflen));
      if (ret && !soap->buflen && r != Z_STREAM_END)
        goto zlib_again;
      ret = soap->buflen;
      if (r == Z_STREAM_END)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflated total %lu->%lu bytes\n", soap->d_stream->total_in, soap->d_stream->total_out));
        soap->z_ratio_in = (float)soap->d_stream->total_in / (float)soap->d_stream->total_out;
        soap->d_stream->next_out = Z_NULL;
      }
      if (soap->count + ret > SOAP_MAXINFLATESIZE && soap->z_ratio_in < SOAP_MINDEFLATERATIO)
      { soap->d_stream->msg = (char*)"caught SOAP_MINDEFLATERATIO explosive decompression guard (remedy: increase SOAP_MAXINFLATESIZE and/or decrease SOAP_MINDEFLATERATIO)";
        return soap->error = SOAP_ZLIB_ERROR;
      }
      DBGLOG(RECV, SOAP_MESSAGE(fdebug, "\n---- decompressed ----\n"));
      DBGMSG(RECV, soap->buf, ret);
#ifndef WITH_LEANER
      if (soap->fpreparerecv && (r = soap->fpreparerecv(soap, soap->buf, ret)))
        return soap->error = r;
#endif
    }
    else
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Unable to inflate: (%d) %s\n", r, soap->d_stream->msg ? soap->d_stream->msg : SOAP_STR_EOS));
      soap->d_stream->next_out = Z_NULL;
      return soap->error = SOAP_ZLIB_ERROR;
    }
  }
#endif
#ifndef WITH_LEANER
  if (soap->fpreparerecv
#ifdef WITH_ZLIB
   && soap->zlib_in == SOAP_ZLIB_NONE
#endif
   && (r = soap->fpreparerecv(soap, soap->buf + soap->bufidx, ret)))
    return soap->error = r;
#endif
  if (ret)
  { soap->count += ret;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Read count=%lu (+%lu)\n", (unsigned long)soap->count, (unsigned long)ret));
    return SOAP_OK;
  }
  return EOF;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_recv(struct soap *soap)
{
#ifndef WITH_LEANER
  if (soap->mode & SOAP_ENC_DIME)
  { if (soap->dime.buflen)
    { char *s;
      int i;
      unsigned char tmp[12];
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "DIME hdr for chunked DIME is in buffer\n"));
      soap->count += soap->dime.buflen - soap->buflen;
      soap->buflen = soap->dime.buflen;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Skip padding (%ld bytes)\n", -(long)soap->dime.size&3));
      for (i = -(long)soap->dime.size&3; i > 0; i--)
      { soap->bufidx++;
        if (soap->bufidx >= soap->buflen)
          if (soap_recv_raw(soap))
            return EOF;
      }
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get DIME hdr for next chunk\n"));
      s = (char*)tmp;
      for (i = 12; i > 0; i--)
      { *s++ = soap->buf[soap->bufidx++];
        if (soap->bufidx >= soap->buflen)
          if (soap_recv_raw(soap))
            return EOF;
      }
      soap->dime.flags = tmp[0] & 0x7;
      soap->dime.size = ((size_t)tmp[8] << 24) | ((size_t)tmp[9] << 16) | ((size_t)tmp[10] << 8) | ((size_t)tmp[11]);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get DIME chunk (%u bytes)\n", (unsigned int)soap->dime.size));
      if (soap->dime.flags & SOAP_DIME_CF)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "More chunking\n"));
        soap->dime.chunksize = soap->dime.size;
        if (soap->buflen - soap->bufidx >= soap->dime.size)
        { soap->dime.buflen = soap->buflen;
          soap->buflen = soap->bufidx + soap->dime.chunksize;
        }
        else
          soap->dime.chunksize -= soap->buflen - soap->bufidx;
      }
      else
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Last chunk\n"));
        soap->dime.buflen = 0;
        soap->dime.chunksize = 0;
      }
      soap->count = soap->buflen - soap->bufidx;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "%u bytes remaining\n", (unsigned int)soap->count));
      return SOAP_OK;
    }
    if (soap->dime.chunksize)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get next DIME hdr for chunked DIME (%u bytes chunk)\n", (unsigned int)soap->dime.chunksize));
      if (soap_recv_raw(soap))
        return EOF;
      if (soap->buflen - soap->bufidx >= soap->dime.chunksize)
      { soap->dime.buflen = soap->buflen;
        soap->count -= soap->buflen - soap->bufidx - soap->dime.chunksize;
        soap->buflen = soap->bufidx + soap->dime.chunksize;
      }
      else
        soap->dime.chunksize -= soap->buflen - soap->bufidx;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "%lu bytes remaining, count=%lu\n", (unsigned long)(soap->buflen-soap->bufidx), (unsigned long)soap->count));
      return SOAP_OK;
    }
  }
  while (soap->ffilterrecv)
  { int err;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Filter recverror = %d\n", soap->recverror));
    if (soap->recverror)
      soap->bufidx = soap->buflen = 0;
    else
    { soap->recverror = soap_recv_raw(soap); /* do not call again after EOF */
      soap->buflen -= soap->bufidx; /* chunked may set bufidx > 0 to skip hex chunk length */
    }
    if ((err = soap->ffilterrecv(soap, soap->buf + soap->bufidx, &soap->buflen, sizeof(soap->buf) - soap->bufidx)))
      return soap->error = err;
    if (soap->buflen)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Filtered %lu\n", (unsigned long)soap->buflen));
      soap->buflen += soap->bufidx;
      return SOAP_OK;
    }
    if (soap->recverror)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Returning postponed EOF%d\n", soap->recverror));
      return soap->recverror;
    }
  }
  return soap->recverror = soap_recv_raw(soap);
#else
  return soap_recv_raw(soap);
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
soap_wchar
SOAP_FMAC2
soap_getchar(struct soap *soap)
{ soap_wchar c;
  c = soap->ahead;
  if (c)
  { if (c != EOF)
      soap->ahead = 0;
    return c;
  }
  return soap_get1(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
const struct soap_code_map*
SOAP_FMAC2
soap_code(const struct soap_code_map *code_map, const char *str)
{ if (code_map && str)
  { while (code_map->string)
    { if (!strcmp(str, code_map->string)) /* case sensitive */
        return code_map;
      code_map++;
    }
  }
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
LONG64
SOAP_FMAC2
soap_code_int(const struct soap_code_map *code_map, const char *str, LONG64 other)
{ if (code_map)
  { while (code_map->string)
    { if (!soap_tag_cmp(str, code_map->string)) /* case insensitive */
        return code_map->code;
      code_map++;
    }
  }
  return other;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_code_str(const struct soap_code_map *code_map, long code)
{ if (!code_map)
    return NULL;
  while (code_map->code != code && code_map->string)
    code_map++;
  return code_map->string;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
LONG64
SOAP_FMAC2
soap_code_bits(const struct soap_code_map *code_map, const char *str)
{ LONG64 bits = 0;
  if (code_map)
  { while (str && *str)
    { const struct soap_code_map *p;
      for (p = code_map; p->string; p++)
      { size_t n = strlen(p->string);
        if (!strncmp(p->string, str, n) && soap_blank((soap_wchar)str[n]))
        { bits |= p->code;
          str += n;
          while (*str > 0 && *str <= 32)
            str++;
          break;
        }
      }
      if (!p->string)
        return 0;
    }
  }
  return bits;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_code_list(struct soap *soap, const struct soap_code_map *code_map, long code)
{ char *t = soap->tmpbuf;
  if (code_map)
  { while (code_map->string)
    { if (code_map->code & code)
      { const char *s = code_map->string;
        if (t != soap->tmpbuf)
          *t++ = ' ';
        while (*s && t < soap->tmpbuf + sizeof(soap->tmpbuf) - 1)
          *t++ = *s++;
        if (t == soap->tmpbuf + sizeof(soap->tmpbuf) - 1)
          break;
      }
      code_map++;
    }
  }
  *t = '\0';
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_1
static soap_wchar
soap_char(struct soap *soap)
{ char tmp[8];
  int i;
  soap_wchar c;
  char *s = tmp;
  for (i = 0; i < 7; i++)
  { c = soap_get1(soap);
    if (c == ';' || (int)c == EOF)
      break;
    *s++ = (char)c;
  }
  *s = '\0';
  if (*tmp == '#')
  { if (tmp[1] == 'x' || tmp[1] == 'X')
      return (soap_wchar)soap_strtol(tmp + 2, NULL, 16);
    return (soap_wchar)soap_strtol(tmp + 1, NULL, 10);
  }
  if (!strcmp(tmp, "lt"))
    return '<';
  if (!strcmp(tmp, "gt"))
    return '>';
  if (!strcmp(tmp, "amp"))
    return '&';
  if (!strcmp(tmp, "quot"))
    return '"';
  if (!strcmp(tmp, "apos"))
    return '\'';
#ifndef WITH_LEAN
  return (soap_wchar)soap_code_int(html_entity_codes, tmp, (LONG64)SOAP_UNKNOWN_CHAR);
#else
  return SOAP_UNKNOWN_CHAR; /* use this to represent unknown code */
#endif
}
#endif

/******************************************************************************/
#ifdef WITH_LEAN
#ifndef PALM_1
soap_wchar
soap_get0(struct soap *soap)
{ if (soap->bufidx >= soap->buflen && soap_recv(soap))
    return EOF;
  return (unsigned char)soap->buf[soap->bufidx];
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_LEAN
#ifndef PALM_1
soap_wchar
soap_get1(struct soap *soap)
{ if (soap->bufidx >= soap->buflen && soap_recv(soap))
    return EOF;
  return (unsigned char)soap->buf[soap->bufidx++];
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
soap_wchar
SOAP_FMAC2
soap_get(struct soap *soap)
{ soap_wchar c;
  c = soap->ahead;
  if (c)
  { if ((int)c != EOF)
      soap->ahead = 0;
  }
  else
    c = soap_get1(soap);
  while ((int)c != EOF)
  { if (soap->cdata)
    { if (c == ']')
      { c = soap_get1(soap);
        if (c == ']')
        { c = soap_get0(soap);
          if (c == '>')
          { soap->cdata = 0;
            c = soap_get1(soap);
            c = soap_get1(soap);
          }
          else
          { soap_unget(soap, ']');
            return ']';
          }
        }
        else
        { soap_revget1(soap);
          return ']';
        }
      }
      else
        return c;
    }
    switch (c)
    { case '<':
        do c = soap_get1(soap);
        while (soap_blank(c));
        if (c == '!' || c == '?' || c == '%')
        { int k = 1;
          if (c == '!')
          { c = soap_get1(soap);
            if (c == '[')
            { do c = soap_get1(soap);
              while ((int)c != EOF && c != '[');
              if ((int)c == EOF)
                break;
              soap->cdata = 1;
              c = soap_get1(soap);
              continue;
            }
            if (c == '-' && (c = soap_get1(soap)) == '-')
            { do
              { c = soap_get1(soap);
                if (c == '-' && (c = soap_get1(soap)) == '-')
                  break;
              } while ((int)c != EOF);
            }
          }
          else if (c == '?')
            c = soap_get_pi(soap);
          while ((int)c != EOF)
          { if (c == '<')
              k++;
            else if (c == '>')
            { if (--k <= 0)
                break;
            }
            c = soap_get1(soap);
          }
          if ((int)c == EOF)
            break;
          c = soap_get1(soap);
          continue;
        }
        if (c == '/')
          return SOAP_TT;
        soap_revget1(soap);
        return SOAP_LT;
      case '>':
        return SOAP_GT;
      case '"':
        return SOAP_QT;
      case '\'':
        return SOAP_AP;
      case '&':
        return soap_char(soap) | 0x80000000;
    }
    break;
  }
  return c;
}
#endif

/******************************************************************************/
#ifndef PALM_1
static soap_wchar
soap_get_pi(struct soap *soap)
{ char buf[64];
  char *s = buf;
  int i = sizeof(buf);
  soap_wchar c = soap_getchar(soap);
  /* This is a quick way to parse XML PI and we could use a callback instead to
   * enable applications to intercept processing instructions */
  while ((int)c != EOF && c != '?')
  { if (--i > 0)
    { if (soap_blank(c))
        c = ' ';
      *s++ = (char)c;
    }
    c = soap_getchar(soap);
  }
  *s = '\0';
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "XML PI <?%s?>\n", buf));
  if (!strncmp(buf, "xml ", 4))
  { s = strstr(buf, " encoding=");
    if (s && s[10])
    { if (!soap_tag_cmp(s + 11, "iso-8859-1*")
       || !soap_tag_cmp(s + 11, "latin1*"))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Switching to latin1 encoding\n"));
        soap->mode |= SOAP_ENC_LATIN;
      }
      else if (!soap_tag_cmp(s + 11, "utf-8*"))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Switching to utf-8 encoding\n"));
        soap->mode &= ~SOAP_ENC_LATIN;
      }
    }
  }
  if ((int)c != EOF)
    c = soap_getchar(soap);
  return c;
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_move(struct soap *soap, size_t n)
{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Moving %lu bytes forward\n", (unsigned long)n));
  for (; n; n--)
    if ((int)soap_getchar(soap) == EOF)
      return SOAP_EOF;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_tell(struct soap *soap)
{ return soap->count - soap->buflen + soap->bufidx - (soap->ahead != 0);
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_pututf8(struct soap *soap, unsigned long c)
{ char tmp[24];
  if ((c < 0x7F && c > 0x1F))
  { *tmp = (char)c;
    return soap_send_raw(soap, tmp, 1);
  }
#ifdef WITH_REPLACE_ILLEGAL_UTF8
  if (!(c == 0x09 || c == 0x0A || c == 0x0D || (c >= 0x80 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF)))
    c = SOAP_UNKNOWN_UNICODE_CHAR;
#endif
#ifndef WITH_LEAN
  if (c > 0x9F)
  { char *t = tmp;
    if (c < 0x0800)
      *t++ = (char)(0xC0 | ((c >> 6) & 0x1F));
    else
    { if (c < 0x010000)
        *t++ = (char)(0xE0 | ((c >> 12) & 0x0F));
      else
      { if (c < 0x200000)
          *t++ = (char)(0xF0 | ((c >> 18) & 0x07));
        else
        { if (c < 0x04000000)
            *t++ = (char)(0xF8 | ((c >> 24) & 0x03));
          else
          { *t++ = (char)(0xFC | ((c >> 30) & 0x01));
            *t++ = (char)(0x80 | ((c >> 24) & 0x3F));
          }
          *t++ = (char)(0x80 | ((c >> 18) & 0x3F));
        }
        *t++ = (char)(0x80 | ((c >> 12) & 0x3F));
      }
      *t++ = (char)(0x80 | ((c >> 6) & 0x3F));
    }
    *t++ = (char)(0x80 | (c & 0x3F));
    *t = '\0';
  }
  else
#endif
    (SOAP_SNPRINTF(tmp, sizeof(tmp), 20), "&#x%lX;", c);
  return soap_send(soap, tmp);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
soap_wchar
SOAP_FMAC2
soap_getutf8(struct soap *soap)
{ soap_wchar c, c1, c2, c3, c4;
  c = soap->ahead;
  if (c >= 0x80)
    soap->ahead = 0;
  else
    c = soap_get(soap);
  if (c < 0x80 || c > 0xFF || (soap->mode & SOAP_ENC_LATIN))
    return c;
  c1 = soap_get1(soap);
  if (c1 < 0x80)
  { soap_revget1(soap); /* doesn't look like this is UTF8 */
    return c;
  }
  c1 &= 0x3F;
  if (c < 0xE0)
    return ((soap_wchar)(c & 0x1F) << 6) | c1;
  c2 = (soap_wchar)soap_get1(soap) & 0x3F;
  if (c < 0xF0)
    return ((soap_wchar)(c & 0x0F) << 12) | (c1 << 6) | c2;
  c3 = (soap_wchar)soap_get1(soap) & 0x3F;
  if (c < 0xF8)
    return ((soap_wchar)(c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
  c4 = (soap_wchar)soap_get1(soap) & 0x3F;
  if (c < 0xFC)
    return ((soap_wchar)(c & 0x03) << 24) | (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
  return ((soap_wchar)(c & 0x01) << 30) | (c1 << 24) | (c2 << 18) | (c3 << 12) | (c4 << 6) | (soap_wchar)(soap_get1(soap) & 0x3F);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_utf8len(const char *s)
{ size_t l = 0;
  while (*s)
    if ((*s++ & 0xC0) != 0x80)
      l++;
  return l;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_puthex(struct soap *soap, const unsigned char *s, int n)
{ char d[2];
  int i;
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { if (!(soap->dom->data = soap_s2hex(soap, s, NULL, n)))
      return soap->error;
    return SOAP_OK;
  }
#endif
  for (i = 0; i < n; i++)
  { int m = *s++;
    d[0] = (char)((m >> 4) + (m > 159 ? '7' : '0'));
    m &= 0x0F;
    d[1] = (char)(m + (m > 9 ? '7' : '0'));
    if (soap_send_raw(soap, d, 2))
      return soap->error;
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
unsigned char*
SOAP_FMAC2
soap_gethex(struct soap *soap, int *n)
{
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { soap->dom->data = soap_string_in(soap, 0, -1, -1, NULL);
    return (unsigned char*)soap_hex2s(soap, soap->dom->data, NULL, 0, n);
  }
#endif
#ifdef WITH_FAST
  soap->labidx = 0;
  for (;;)
  { char *s;
    size_t i, k;
    if (soap_append_lab(soap, NULL, 0))
      return NULL;
    s = soap->labbuf + soap->labidx;
    k = soap->lablen - soap->labidx;
    soap->labidx = soap->lablen;
    for (i = 0; i < k; i++)
    { char d1, d2;
      soap_wchar c;
      c = soap_get(soap);
      if (soap_isxdigit(c))
      { d1 = (char)c;
        c = soap_get(soap);
        if (soap_isxdigit(c))
          d2 = (char)c;
        else
        { soap->error = SOAP_TYPE;
          return NULL;
        }
      }
      else
      { unsigned char *p;
	size_t l = soap->lablen + i - k;
        soap_unget(soap, c);
        if (n)
          *n = (int)l;
        p = (unsigned char*)soap_malloc(soap, l);
        if (p)
          soap_memcpy((void*)p, l, (const void*)soap->labbuf, l);
        return p;
      }
      *s++ = (char)(((d1 >= 'A' ? (d1 & 0x7) + 9 : d1 - '0') << 4) + (d2 >= 'A' ? (d2 & 0x7) + 9 : d2 - '0'));
    }
  }
#else
  if (soap_new_block(soap) == NULL)
    return NULL;
  for (;;)
  { int i;
    char *s = (char*)soap_push_block(soap, NULL, SOAP_BLKLEN);
    if (!s)
    { soap_end_block(soap, NULL);
      return NULL;
    }
    for (i = 0; i < SOAP_BLKLEN; i++)
    { char d1, d2;
      soap_wchar c = soap_get(soap);
      if (soap_isxdigit(c))
      { d1 = (char)c;
        c = soap_get(soap);
        if (soap_isxdigit(c))
          d2 = (char)c;
        else
        { soap_end_block(soap, NULL);
          soap->error = SOAP_TYPE;
          return NULL;
        }
      }
      else
      { unsigned char *p;
        soap_unget(soap, c);
        if (n)
          *n = (int)soap_size_block(soap, NULL, i);
        p = (unsigned char*)soap_save_block(soap, NULL, NULL, 0);
        return p;
      }
      *s++ = ((d1 >= 'A' ? (d1 & 0x7) + 9 : d1 - '0') << 4) + (d2 >= 'A' ? (d2 & 0x7) + 9 : d2 - '0');
    }
  }
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_putbase64(struct soap *soap, const unsigned char *s, int n)
{ int i;
  unsigned long m;
  char d[4];
  if (!s)
    return SOAP_OK;
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { if (!(soap->dom->data = soap_s2base64(soap, s, NULL, n)))
      return soap->error;
    return SOAP_OK;
  }
#endif
  for (; n > 2; n -= 3, s += 3)
  { m = s[0];
    m = (m << 8) | s[1];
    m = (m << 8) | s[2];
    for (i = 4; i > 0; m >>= 6)
      d[--i] = soap_base64o[m & 0x3F];
    if (soap_send_raw(soap, d, 4))
      return soap->error;
  }
  if (n > 0)
  { m = 0;
    for (i = 0; i < n; i++)
      m = (m << 8) | *s++;
    for (; i < 3; i++)
      m <<= 8;
    for (i++; i > 0; m >>= 6)
      d[--i] = soap_base64o[m & 0x3F];
    for (i = 3; i > n; i--)
      d[i] = '=';
    if (soap_send_raw(soap, d, 4))
      return soap->error;
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
unsigned char*
SOAP_FMAC2
soap_getbase64(struct soap *soap, int *n, int malloc_flag)
{ (void)malloc_flag;
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { soap->dom->data = soap_string_in(soap, 0, -1, -1, NULL);
    return (unsigned char*)soap_base642s(soap, soap->dom->data, NULL, 0, n);
  }
#endif
#ifdef WITH_FAST
  soap->labidx = 0;
  for (;;)
  { size_t i, k;
    char *s;
    if (soap_append_lab(soap, NULL, 2))
      return NULL;
    s = soap->labbuf + soap->labidx;
    k = soap->lablen - soap->labidx;
    soap->labidx = 3 * (soap->lablen / 3);
    if (!s)
      return NULL;
    if (k > 2)
    { for (i = 0; i < k - 2; i += 3)
      { unsigned long m = 0;
        int j = 0;
        do
        { soap_wchar c = soap_get(soap);
          if (c < SOAP_AP)
            c &= 0x7FFFFFFF;
          if (c == '=' || c < 0)
          { unsigned char *p;
	    size_t l;
            switch (j)
            { case 2:
                *s++ = (char)((m >> 4) & 0xFF);
                i++;
                break;
              case 3:
                *s++ = (char)((m >> 10) & 0xFF);
                *s++ = (char)((m >> 2) & 0xFF);
                i += 2;
            }
	    l = soap->lablen + i - k;
            if (n)
              *n = (int)l;
            p = (unsigned char*)soap_malloc(soap, l);
            if (p)
              soap_memcpy((void*)p, l, (const void*)soap->labbuf, l);
            if (c >= 0)
            { while ((int)((c = soap_get(soap)) != EOF) && c != SOAP_LT && c != SOAP_TT)
                continue;
            }
            soap_unget(soap, c);
            return p;
          }
          c -= '+';
          if (c >= 0 && c <= 79)
          { int b = soap_base64i[c];
            if (b >= 64)
            { soap->error = SOAP_TYPE;
              return NULL;
            }
            m = (m << 6) + b;
            j++;
          }
          else if (!soap_blank(c + '+'))
          { soap->error = SOAP_TYPE;
            return NULL;
          }
        } while (j < 4);
        *s++ = (char)((m >> 16) & 0xFF);
        *s++ = (char)((m >> 8) & 0xFF);
        *s++ = (char)(m & 0xFF);
      }
    }
  }
#else
  if (soap_new_block(soap) == NULL)
    return NULL;
  for (;;)
  { int i;
    char *s = (char*)soap_push_block(soap, NULL, 3 * SOAP_BLKLEN); /* must be multiple of 3 */
    if (!s)
    { soap_end_block(soap, NULL);
      return NULL;
    }
    for (i = 0; i < SOAP_BLKLEN; i++)
    { unsigned long m = 0;
      int j = 0;
      do
      { soap_wchar c = soap_get(soap);
        if (c == '=' || c < 0)
        { unsigned char *p;
          i *= 3;
          switch (j)
          { case 2:
              *s++ = (char)((m >> 4) & 0xFF);
              i++;
              break;
            case 3:
              *s++ = (char)((m >> 10) & 0xFF);
              *s++ = (char)((m >> 2) & 0xFF);
              i += 2;
          }
          if (n)
            *n = (int)soap_size_block(soap, NULL, i);
          p = (unsigned char*)soap_save_block(soap, NULL, NULL, 0);
          if (c >= 0)
          { while ((int)((c = soap_get(soap)) != EOF) && c != SOAP_LT && c != SOAP_TT)
              continue;
          }
          soap_unget(soap, c);
          return p;
        }
        c -= '+';
        if (c >= 0 && c <= 79)
        { int b = soap_base64i[c];
          if (b >= 64)
          { soap->error = SOAP_TYPE;
            return NULL;
          }
          m = (m << 6) + b;
          j++;
        }
        else if (!soap_blank(c))
        { soap->error = SOAP_TYPE;
          return NULL;
        }
      } while (j < 4);
      *s++ = (char)((m >> 16) & 0xFF);
      *s++ = (char)((m >> 8) & 0xFF);
      *s++ = (char)(m & 0xFF);
    }
  }
#endif
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_xop_forward(struct soap *soap, unsigned char **ptr, int *size, char **id, char **type, char **options)
{ /* Check MTOM xop:Include element (within hex/base64Binary) */
  /* TODO: this code to be obsoleted with new import/xop.h conventions */
  short body = soap->body; /* should save type too? */
  if (!soap_peek_element(soap))
  { if (!soap_element_begin_in(soap, "xop:Include", 0, NULL))
    { if (soap_attachment_forward(soap, ptr, size, id, type, options)
       || (soap->body && soap_element_end_in(soap, "xop:Include")))
        return soap->error;
    }
  }
  soap->body = body;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_attachment_forward(struct soap *soap, unsigned char **ptr, int *size, char **id, char **type, char **options)
{ struct soap_xlist *xp;
  *ptr = NULL;
  *size = 0;
  *id = NULL;
  *type = NULL;
  *options = NULL;
  if (!*soap->href)
    return SOAP_OK;
  *id = soap_strdup(soap, soap->href);
  xp = (struct soap_xlist*)SOAP_MALLOC(soap, sizeof(struct soap_xlist));
  if (!xp)
    return soap->error = SOAP_EOM;
  xp->next = soap->xlist;
  xp->ptr = ptr;
  xp->size = size;
  xp->id = *id;
  xp->type = type;
  xp->options = options;
  soap->xlist = xp;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void *
SOAP_FMAC2
soap_memdup(struct soap *soap, const void *s, size_t n)
{ void *t = NULL;
  if (s && (t = soap_malloc(soap, n)))
    soap_memcpy(t, n, s, n);
  return t;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
char *
SOAP_FMAC2
soap_strdup(struct soap *soap, const char *s)
{ char *t = NULL;
  if (s)
  { size_t l = strlen(s) + 1;
    if ((t = (char*)soap_malloc(soap, l)))
      soap_memcpy((void*)t, l, (const void*)s, l);
  }
  return t;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
wchar_t *
SOAP_FMAC2
soap_wstrdup(struct soap *soap, const wchar_t *s)
{ wchar_t *t = NULL;
  if (s)
  { size_t n = 0;
    while (s[n])
      n++;
    n = sizeof(wchar_t)*(n+1);
    if ((t = (wchar_t*)soap_malloc(soap, n)))
      soap_memcpy((void*)t, n, (const void*)s, n);
  }
  return t;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
struct soap_blist*
SOAP_FMAC2
soap_new_block(struct soap *soap)
{ struct soap_blist *p;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New block sequence (prev=%p)\n", soap->blist));
  if (!(p = (struct soap_blist*)SOAP_MALLOC(soap, sizeof(struct soap_blist))))
  { soap->error = SOAP_EOM;
    return NULL;
  }
  p->next = soap->blist;
  p->head = NULL;
  p->size = 0;
  soap->blist = p;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void*
SOAP_FMAC2
soap_push_block(struct soap *soap, struct soap_blist *b, size_t n)
{ struct soap_bhead *p;
  if (!b)
    b = soap->blist;
  if (!(p = (struct soap_bhead*)SOAP_MALLOC(soap, sizeof(struct soap_bhead) + n)))
  { soap->error = SOAP_EOM;
    return NULL;
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Push block %p of %u bytes (%u bytes total)\n", p, (unsigned int)n, (unsigned int)b->size + (unsigned int)n));
  p->next = b->head;
  b->head = p;
  p->size = n;
  b->size += n;
  return (void*)(p + 1); /* skip block header and point to n allocated bytes */
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_pop_block(struct soap *soap, struct soap_blist *b)
{ struct soap_bhead *p;
  if (!b)
    b = soap->blist;
  if (!b->head)
    return;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Pop block\n"));
  p = b->head;
  b->size -= p->size;
  b->head = p->next;
  SOAP_FREE(soap, p);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_update_pointers(struct soap *soap, const char *dst, const char *src, size_t len)
{ const void *start = src, *end = src + len;
#ifndef WITH_LEANER
  struct soap_xlist *xp;
#endif
#ifndef WITH_NOIDREF
  if ((soap->version && !(soap->imode & SOAP_XML_TREE)) || (soap->mode & SOAP_XML_GRAPH))
  { int i;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Update pointers %p (%lu bytes) -> %p\n", src, (unsigned long)len, dst));
    for (i = 0; i < SOAP_IDHASH; i++)
    { struct soap_ilist *ip;
      for (ip = soap->iht[i]; ip; ip = ip->next)
      { struct soap_flist *fp;
	void *p, **q;
	if (ip->shaky)
	{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Update shaky id='%s'\n", ip->id));
	  if (ip->ptr && ip->ptr >= start && ip->ptr < end)
	  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Update ptr %p -> %p\n", ip->ptr, (const char*)ip->ptr + (dst-src)));
	    ip->ptr = (void*)((const char*)ip->ptr + (dst-src));
	  }
	  for (q = &ip->link; q; q = (void**)p)
	  { p = *q;
	    if (p && p >= start && p < end)
	    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Link update id='%s' %p -> %p\n", ip->id, p, (const char*)p + (dst-src)));
	      *q = (void*)((const char*)p + (dst-src));
	    }
	  }
	  for (q = &ip->copy; q; q = (void**)p)
	  { p = *q;
	    if (p && p >= start && p < end)
	    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Copy chain update id='%s' %p -> %p\n", ip->id, p, (const char*)p + (dst-src)));
	      *q = (void*)((const char*)p + (dst-src));
	    }
	  }
	  for (fp = ip->flist; fp; fp = fp->next)
	  { if (fp->ptr >= start && fp->ptr < end)
	    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Copy list update id='%s' target type=%d %p -> %p\n", ip->id, fp->type, fp->ptr, (char*)fp->ptr + (dst-src)));
	      fp->ptr = (void*)((const char*)fp->ptr + (dst-src));
	    }
	  }
	  if (ip->smart && ip->smart >= start && ip->smart < end)
	  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Smart shared pointer update %p -> %p\n", ip->smart, (const char*)ip->smart + (dst-src)));
	    ip->smart = (void*)((const char*)ip->smart + (dst-src));
	  }
	}
      }
    }
  }
#else
  (void)soap; (void)start; (void)end; (void)dst; (void)src;
#endif
#ifndef WITH_LEANER
  for (xp = soap->xlist; xp; xp = xp->next)
  { if (xp->ptr && (void*)xp->ptr >= start && (void*)xp->ptr < end)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Update attachment id='%s' %p -> %p\n", xp->id ? xp->id : SOAP_STR_EOS, xp->ptr, (char*)xp->ptr + (dst-src)));
      xp->ptr = (unsigned char**)((char*)xp->ptr + (dst-src));
      xp->size = (int*)((char*)xp->size + (dst-src));
      xp->type = (char**)((char*)xp->type + (dst-src));
      xp->options = (char**)((char*)xp->options + (dst-src));
    }
  }
#endif
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_1
static int
soap_has_copies(struct soap *soap, const char *start, const char *end)
{ int i;
  struct soap_ilist *ip = NULL;
  struct soap_flist *fp = NULL;
  const char *p;
  for (i = 0; i < SOAP_IDHASH; i++)
  { for (ip = soap->iht[i]; ip; ip = ip->next)
    { for (p = (const char*)ip->copy; p; p = *(const char**)p)
        if (p >= start && p < end)
          return SOAP_ERR;
      for (fp = ip->flist; fp; fp = fp->next)
        if (fp->type == ip->type && (const char*)fp->ptr >= start && (const char*)fp->ptr < end)
          return SOAP_ERR;
    }
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_resolve(struct soap *soap)
{ int i;
  short flag;
  const char *id;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolving forwarded refs\n"));
  for (i = 0; i < SOAP_IDHASH; i++)
  { struct soap_ilist *ip;
    for (ip = soap->iht[i]; ip; ip = ip->next)
    { if (ip->ptr)
      { void **q;
	struct soap_flist *fp, **fpp = &ip->flist;
	if (ip->spine)
	  ip->spine[0] = ip->ptr;
        q = (void**)ip->link;
        ip->link = NULL;
        DBGLOG(TEST, if (q) SOAP_MESSAGE(fdebug, "Traversing link chain to resolve id='%s' type=%d\n", ip->id, ip->type));
        while (q)
        { void *p = *q;
          *q = ip->ptr;
          DBGLOG(TEST, SOAP_MESSAGE(fdebug, "... link %p -> %p\n", q, ip->ptr));
          q = (void**)p;
        }
	while ((fp = *fpp))
	{ if (fp->level > 0 && fp->finsert)
	  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "... insert type=%d link %p -> %p\n", fp->type, fp->ptr, ip->ptr));
	    if (ip->spine && fp->level <= SOAP_MAXPTRS)
	      fp->finsert(soap, ip->type, fp->type, fp->ptr, fp->index, &ip->spine[fp->level - 1], &ip->smart);
	    else if (fp->level == 1)
	      fp->finsert(soap, ip->type, fp->type, fp->ptr, fp->index, &ip->ptr, &ip->smart);
	    else if (fp->level <= SOAP_MAXPTRS)
	    { int i;
	      if (!(ip->spine = (void**)soap_malloc(soap, SOAP_MAXPTRS * sizeof(void*))))
	        return soap->error = SOAP_EOM;
	      ip->spine[0] = ip->ptr;
	      for (i = 1; i < SOAP_MAXPTRS; i++)
		ip->spine[i] = &ip->spine[i - 1];
	      fp->finsert(soap, ip->type, fp->type, fp->ptr, fp->index, &ip->spine[fp->level - 1], &ip->smart);
	    }
	    *fpp = fp->next;
	    SOAP_FREE(soap, fp);
	  }
	  else
	    fpp = &fp->next;
	}
      }
      else if (*ip->id == '#')
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Missing id='%s'\n", ip->id));
        soap_strcpy(soap->id, sizeof(soap->id), ip->id + 1);
        return soap->error = SOAP_MISSING_ID;
      }
    }
  }
  do
  { flag = 0;
    id = NULL;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolution phase\n"));
    for (i = 0; i < SOAP_IDHASH; i++)
    { struct soap_ilist *ip;
      for (ip = soap->iht[i]; ip; ip = ip->next)
      { if (ip->copy || ip->flist)
	{ if (ip->ptr && !soap_has_copies(soap, (const char*)ip->ptr, (const char*)ip->ptr + ip->size))
	  { struct soap_flist *fp;
	    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolving id='%s' type=%d ptr=%p size=%lu ...\n", ip->id, ip->type, ip->ptr, (unsigned long)ip->size));
	    if (ip->copy)
	    { void *p, **q = (void**)ip->copy;
	      DBGLOG(TEST, if (q) SOAP_MESSAGE(fdebug, "Traversing copy chain to resolve id='%s'\n", ip->id));
	      ip->copy = NULL;
	      do
	      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "... copy %p -> %p (%u bytes)\n", ip->ptr, q, (unsigned int)ip->size));
		p = *q;
		soap_memcpy((void*)q, ip->size, (const void*)ip->ptr, ip->size);
		q = (void**)p;
	      } while (q);
	      flag = 1;
	    }
	    while ((fp = ip->flist))
	    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolving forwarded data type=%d target type=%d location=%p level=%u id='%s'\n", ip->type, fp->type, ip->ptr, fp->level, ip->id));
	      if (fp->level == 0)
	      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "... copy %p -> %p (%lu bytes)\n", ip->ptr, fp->ptr, (unsigned long)ip->size));
		if (fp->finsert)
		  fp->finsert(soap, ip->type, fp->type, fp->ptr, fp->index, ip->ptr, &ip->smart);
		else
		  soap_memcpy((void*)fp->ptr, ip->size, (const void*)ip->ptr, ip->size);
	      }
	      ip->flist = fp->next;
	      SOAP_FREE(soap, fp);
	      flag = 1;
	    }
	  }
	  else
	    id = ip->id;
	}
      }
    }
  } while (flag);
  if (id)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolution error: forwarded data for id='%s' could not be propagated, please report this problem to the gSOAP developers\n", id));
    return soap_id_nullify(soap, id);
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolution done\n"));
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_size_block(struct soap *soap, struct soap_blist *b, size_t n)
{ if (!b)
    b = soap->blist;
  if (b->head)
  { b->size -= b->head->size - n;
    b->head->size = n;
  }
  return b->size;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
char*
SOAP_FMAC2
soap_first_block(struct soap *soap, struct soap_blist *b)
{ struct soap_bhead *p, *q, *r;
  if (!b)
    b = soap->blist;
  p = b->head;
  if (!p)
    return NULL;
  r = NULL;
  do
  { q = p->next;
    p->next = r;
    r = p;
    p = q;
  } while (p);
  b->head = r;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "First block %p\n", r + 1));
  return (char*)(r + 1);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
char*
SOAP_FMAC2
soap_next_block(struct soap *soap, struct soap_blist *b)
{ struct soap_bhead *p;
  if (!b)
    b = soap->blist;
  p = b->head;
  if (p)
  { b->head = p->next;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Next block %p, deleting current block\n", b->head + 1));
    SOAP_FREE(soap, p);
    if (b->head)
      return (char*)(b->head + 1);
  }
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_block_size(struct soap *soap, struct soap_blist *b)
{ if (!b)
    b = soap->blist;
  return b->head->size;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_end_block(struct soap *soap, struct soap_blist *b)
{ struct soap_bhead *p, *q;
  if (!b)
    b = soap->blist;
  if (b)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End of block sequence, free all remaining blocks\n"));
    for (p = b->head; p; p = q)
    { q = p->next;
      SOAP_FREE(soap, p);
    }
    if (soap->blist == b)
      soap->blist = b->next;
    else
    { struct soap_blist *bp;
      for (bp = soap->blist; bp; bp = bp->next)
      { if (bp->next == b)
        { bp->next = b->next;
          break;
        }
      }
    }
    SOAP_FREE(soap, b);
  }
  DBGLOG(TEST, if (soap->blist) SOAP_MESSAGE(fdebug, "Restored previous block sequence\n"));
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
char*
SOAP_FMAC2
soap_save_block(struct soap *soap, struct soap_blist *b, char *p, int flag)
{ size_t n;
  char *q, *s;
  if (!b)
    b = soap->blist;
  if (b->size)
  { if (!p)
      p = (char*)soap_malloc(soap, b->size);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Save all blocks in contiguous memory space of %u bytes (%p->%p)\n", (unsigned int)b->size, b->head, p));
    if (p)
    { s = p;
      for (q = soap_first_block(soap, b); q; q = soap_next_block(soap, b))
      { n = soap_block_size(soap, b);
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Copy %u bytes from %p to %p\n", (unsigned int)n, q, s));
        if (flag)
	  soap_update_pointers(soap, s, q, n);
	soap_memcpy((void*)s, n, (const void*)q, n);
        s += n;
      }
    }
    else
      soap->error = SOAP_EOM;
  }
  soap_end_block(soap, b);
#ifndef WITH_NOIDREF
  if (!soap->blist && ((soap->version && !(soap->imode & SOAP_XML_TREE)) || (soap->mode & SOAP_XML_GRAPH)))
  { int i;
    struct soap_ilist *ip = NULL;
    for (i = 0; i < SOAP_IDHASH; i++)
      for (ip = soap->iht[i]; ip; ip = ip->next)
	ip->shaky = 0;
  }
#endif
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
char *
SOAP_FMAC2
soap_putsizesoffsets(struct soap *soap, const char *type, const int *size, const int *offset, int dim)
{ int i;
  const char *t = ",%d";
  if (!type)
    return NULL;
  if (soap->version == 2)
    t = " %d";
  if (soap->version != 2 && offset)
  { (SOAP_SNPRINTF(soap->type, sizeof(soap->type) - 1, strlen(type) + 20), "%s[%d", type, size[0] + offset[0]);
    for (i = 1; i < dim; i++)
    { size_t l = strlen(soap->type);
      (SOAP_SNPRINTF(soap->type + l, sizeof(soap->type) - l - 1, 20), t, size[i] + offset[i]);
    }
  }
  else
  { (SOAP_SNPRINTF(soap->type, sizeof(soap->type) - 1, strlen(type) + 20), "%s[%d", type, size[0]);
    for (i = 1; i < dim; i++)
    { size_t l = strlen(soap->type);
      (SOAP_SNPRINTF(soap->type + l, sizeof(soap->type) - l - 1, 20), t, size[i]);
    }
  }
  soap_strncat(soap->type, sizeof(soap->type), "]", 1);
  return soap->type;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
char *
SOAP_FMAC2
soap_putoffsets(struct soap *soap, const int *offset, int dim)
{ int i;
  soap->arrayOffset[0] = '\0';
  if (soap->version == 1)
  { (SOAP_SNPRINTF(soap->arrayOffset, sizeof(soap->arrayOffset) - 1, 20), "[%d", offset[0]);
    for (i = 1; i < dim; i++)
    { size_t l = strlen(soap->arrayOffset);
      (SOAP_SNPRINTF(soap->arrayOffset + l, sizeof(soap->arrayOffset) - l - 1, 20), ",%d", offset[i]);
    }
    soap_strncat(soap->arrayOffset, sizeof(soap->arrayOffset), "]", 1);
  }
  return soap->arrayOffset;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_size(const int *size, int dim)
{ int i;
  size_t n = 0;
  if (size[0] <= 0)
    return 0;
  n = (size_t)size[0];
  for (i = 1; i < dim; i++)
  { if (size[i] <= 0)
      return 0;
    n *= (size_t)size[i];
  }
  return (size_t)n;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_getsizes(const char *attr, int *size, int dim)
{ size_t i, k, n;
  if (!*attr)
    return 0;
  i = strlen(attr);
  n = 1;
  do
  { for (; i > 0; i--)
      if (attr[i - 1] == '[' || attr[i - 1] == ',' || attr[i - 1] == ' ')
        break;
    n *= k = (size_t)soap_strtoul(attr + i, NULL, 10);
    size[--dim] = (int)k;
    if (n > SOAP_MAXARRAYSIZE)
      return 0;
  } while (dim > 0 && --i > 0 && attr[i] != '[');
  return n;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_getoffsets(const char *attr, const int *size, int *offset, int dim)
{ int i, j = 0;
  if (offset)
  { for (i = 0; i < dim && attr && *attr; i++)
    { attr++;
      j *= size[i];
      j += offset[i] = (int)soap_strtol(attr, NULL, 10);
      attr = strchr(attr, ',');
    }
  }
  else
  { for (i = 0; i < dim && attr && *attr; i++)
    { attr++;
      j *= size[i];
      j += (int)soap_strtol(attr, NULL, 10);
      attr = strchr(attr, ',');
    }
  }
  return j;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_getposition(const char *attr, int *pos)
{ int i, n;
  if (!*attr)
    return -1;
  n = 0;
  i = 1;
  do
  { pos[n++] = (int)soap_strtol(attr + i, NULL, 10);
    while (attr[i] && attr[i] != ',' && attr[i] != ']')
      i++;
    if (attr[i] == ',')
      i++;
  } while (n < SOAP_MAXDIMS && attr[i] && attr[i] != ']');
  return n;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
struct soap_nlist *
SOAP_FMAC2
soap_push_namespace(struct soap *soap, const char *id, const char *ns)
{ struct soap_nlist *np;
  struct Namespace *p;
  short i = -1;
  size_t n, k;
  n = strlen(id);
  k = strlen(ns) + 1;
  p = soap->local_namespaces;
  if (p)
  { for (i = 0; p->id; p++, i++)
    { if (p->ns && !strcmp(ns, p->ns))
        break;
      if (p->out)
      { if (!strcmp(ns, p->out))
          break;
      }
      else if (p->in)
      { if (!soap_tag_cmp(ns, p->in))
        { if ((p->out = (char*)SOAP_MALLOC(soap, k)))
            soap_strcpy(p->out, k, ns);
          break;
        }
      }
    }
    if (!p || !p->id)
      i = -1;
  }
  if (i >= 0)
    k = 0;
  np = (struct soap_nlist*)SOAP_MALLOC(soap, sizeof(struct soap_nlist) + n + k);
  if (!np)
  { soap->error = SOAP_EOM;
    return NULL;
  }
  np->next = soap->nlist;
  soap->nlist = np;
  np->level = soap->level;
  np->index = i;
  soap_strcpy((char*)np->id, n + 1, id);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Push namespace binding (level=%u) '%s'='%s'\n", soap->level, id, ns));
  if (i < 0)
  { np->ns = np->id + n + 1;
    soap_strcpy((char*)np->ns, k, ns);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Push NOT OK: no match found for '%s' in namespace mapping table (added to stack anyway)\n", ns));
  }
  else
  { np->ns = NULL;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Push OK ('%s' matches '%s' in namespace table)\n", id, p->id));
  }
  return np;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_pop_namespace(struct soap *soap)
{ struct soap_nlist *np, *nq;
  for (np = soap->nlist; np && np->level >= soap->level; np = nq)
  { nq = np->next;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Pop namespace binding (level=%u) '%s' level=%u\n", soap->level, np->id, np->level));
    SOAP_FREE(soap, np);
  }
  soap->nlist = np;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_match_namespace(struct soap *soap, const char *id1, const char *id2, size_t n1, size_t n2)
{ struct soap_nlist *np = soap->nlist;
  const char *s;
  while (np && (strncmp(np->id, id1, n1) || np->id[n1]))
    np = np->next;
  if (np)
  { if (!(soap->mode & SOAP_XML_IGNORENS))
      if (np->index < 0
       || ((s = soap->local_namespaces[np->index].id) && (strncmp(s, id2, n2) || (s[n2] && s[n2] != '_'))))
        return SOAP_NAMESPACE;
    return SOAP_OK;
  }
  if (n1 == 0)
    return (soap->mode & SOAP_XML_IGNORENS) ? SOAP_OK : SOAP_NAMESPACE;
  if ((n1 == 3 && n1 == n2 && !strncmp(id1, "xml", 3) && !strncmp(id1, id2, 3))
   || (soap->mode & SOAP_XML_IGNORENS))
    return SOAP_OK;
  return soap->error = SOAP_SYNTAX_ERROR;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_current_namespace(struct soap *soap, const char *tag)
{ struct soap_nlist *np;
  const char *s;
  if (!tag || !strncmp(tag, "xml", 3))
    return NULL;
  np = soap->nlist;
  if (!(s = strchr(tag, ':')))
  { while (np && *np->id) /* find default namespace, if present */
      np = np->next;
  }
  else
  { while (np && (strncmp(np->id, tag, s - tag) || np->id[s - tag]))
      np = np->next;
    if (!np)
      soap->error = SOAP_NAMESPACE;
  }
  if (np)
  { if (np->index >= 0)
      return soap->namespaces[np->index].ns;
    return soap_strdup(soap, np->ns);
  }
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_tag_cmp(const char *s, const char *t)
{ for (;;)
  { int c1 = *s;
    int c2 = *t;
    if (!c1 || c1 == '"')
      break;
    if (c2 != '-')
    { if (c1 != c2)
      { if (c1 >= 'A' && c1 <= 'Z')
          c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z')
          c2 += 'a' - 'A';
      }
      if (c1 != c2)
      { if (c2 != '*')
          return 1;
        c2 = *++t;
        if (!c2)
          return 0;
        if (c2 >= 'A' && c2 <= 'Z')
          c2 += 'a' - 'A';
        for (;;)
        { c1 = *s;
          if (!c1 || c1 == '"')
            break;
          if (c1 >= 'A' && c1 <= 'Z')
            c1 += 'a' - 'A';
          if (c1 == c2 && !soap_tag_cmp(s + 1, t + 1))
            return 0;
          s++;
        }
        break;
      }
    }
    s++;
    t++;
  }
  if (*t == '*' && !t[1])
    return 0;
  return *t;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_match_tag(struct soap *soap, const char *tag1, const char *tag2)
{ const char *s, *t;
  int err;
  if (!tag1 || !tag2 || !*tag2)
    return SOAP_OK;
  s = strchr(tag1, ':');
  t = strchr(tag2, ':');
  if (t)
  { if (s)
    { if (t[1] && SOAP_STRCMP(s + 1, t + 1))
        return SOAP_TAG_MISMATCH;
      if (t != tag2 && (err = soap_match_namespace(soap, tag1, tag2, s - tag1, t - tag2)))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Tags '%s' and '%s' match but namespaces differ\n", tag1, tag2));
        if (err == SOAP_NAMESPACE)
          return SOAP_TAG_MISMATCH;
        return err;
      }
    }
    else if (!t[1])
    { if ((soap->mode & SOAP_XML_IGNORENS) || soap_match_namespace(soap, tag1, tag2, 0, t - tag2))
        return SOAP_TAG_MISMATCH;
    }
    else if (SOAP_STRCMP(tag1, t + 1))
    { return SOAP_TAG_MISMATCH;
    }
    else if (t != tag2 && (err = soap_match_namespace(soap, tag1, tag2, 0, t - tag2)))
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Tags '%s' and '%s' match but namespaces differ\n", tag1, tag2));
      if (err == SOAP_NAMESPACE)
        return SOAP_TAG_MISMATCH;
      return err;
    }
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Tags and (default) namespaces match: '%s' '%s'\n", tag1, tag2));
    return SOAP_OK;
  }
  if (s)
  { if (SOAP_STRCMP(s + 1, tag2))
      return SOAP_TAG_MISMATCH;
  }
  else if (SOAP_STRCMP(tag1, tag2))
    return SOAP_TAG_MISMATCH;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Tags match: '%s' '%s'\n", tag1, tag2));
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_match_array(struct soap *soap, const char *type)
{ if (*soap->arrayType)
    if (soap_match_tag(soap, soap->arrayType, type)
     && soap_match_tag(soap, soap->arrayType, "xsd:anyType")
     && soap_match_tag(soap, soap->arrayType, "xsd:ur-type")
    )
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Array type mismatch: '%s' '%s'\n", soap->arrayType, type));
      return SOAP_TAG_MISMATCH;
    }
  return SOAP_OK;
}
#endif

/******************************************************************************\
 *
 *	SSL/TLS
 *
\******************************************************************************/

/******************************************************************************/
#ifdef WITH_OPENSSL
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_rand()
{ unsigned char buf[4];
  if (!soap_ssl_init_done)
    soap_ssl_init();
  RAND_pseudo_bytes(buf, 4);
  return *(int*)buf;
}
#endif
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS) || defined(WITH_SYSTEMSSL)
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
soap_ssl_server_context(struct soap *soap, unsigned short flags, const char *keyfile, const char *keyid, const char *password, const char *cafile, const char *capath, const char *dhfile, const char *randfile, const char *sid)
#else
soap_ssl_server_context(struct soap *soap, unsigned short flags, const char *keyfile, const char *password, const char *cafile, const char *capath, const char *dhfile, const char *randfile, const char *sid)
#endif
{ int err;
  soap->keyfile = keyfile;
#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
  soap->keyid = keyid;
#endif
  soap->password = password;
  soap->cafile = cafile;
  soap->capath = capath;
#ifdef WITH_OPENSSL
  soap->dhfile = dhfile;
  soap->randfile = randfile;
  if (!soap->fsslverify)
    soap->fsslverify = ssl_verify_callback;
#endif
  soap->ssl_flags = flags | (dhfile == NULL ? SOAP_SSL_RSA : 0);
#ifdef WITH_GNUTLS
  (void)randfile; (void)sid;
  if (dhfile)
  { char *s;
    int n = (int)soap_strtoul(dhfile, &s, 10);
    if (!soap->dh_params)
      gnutls_dh_params_init(&soap->dh_params);
    /* if dhfile is numeric, treat it as a key length to generate DH params which can take a while */
    if (n >= 512 && s && *s == '\0')
      gnutls_dh_params_generate2(soap->dh_params, (unsigned int)n);
    else
    { unsigned int dparams_len;
      unsigned char dparams_buf[1024];
      FILE *fd = fopen(dhfile, "r");
      if (!fd)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Invalid DH file", SOAP_SSL_ERROR);
      dparams_len = (unsigned int)fread(dparams_buf, 1, sizeof(dparams_buf), fd);
      fclose(fd);
      gnutls_datum_t dparams = { dparams_buf, dparams_len };
      if (gnutls_dh_params_import_pkcs3(soap->dh_params, &dparams, GNUTLS_X509_FMT_PEM))
        return soap_set_receiver_error(soap, "SSL/TLS error", "Invalid DH file", SOAP_SSL_ERROR);
    }
  }
  else
  { if (!soap->rsa_params)
      gnutls_rsa_params_init(&soap->rsa_params);
    gnutls_rsa_params_generate2(soap->rsa_params, SOAP_SSL_RSA_BITS);
  }
  if (soap->session)
  { gnutls_deinit(soap->session);
    soap->session = NULL;
  }
  if (soap->xcred)
  { gnutls_certificate_free_credentials(soap->xcred);
    soap->xcred = NULL;
  }
#endif
#ifdef WITH_SYSTEMSSL
  (void)randfile; (void)sid;
  if (soap->ctx)
    gsk_environment_close(&soap->ctx);
#endif
  err = soap->fsslauth(soap);
#ifdef WITH_OPENSSL
  if (!err)
  { if (sid)
      SSL_CTX_set_session_id_context(soap->ctx, (unsigned char*)sid, (unsigned int)strlen(sid));
    else
      SSL_CTX_set_session_cache_mode(soap->ctx, SSL_SESS_CACHE_OFF);
  }
#endif
  return err;
}
#endif
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS) || defined(WITH_SYSTEMSSL)
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
soap_ssl_client_context(struct soap *soap, unsigned short flags, const char *keyfile, const char *keyid, const char *password, const char *cafile, const char *capath, const char *randfile)
#else
soap_ssl_client_context(struct soap *soap, unsigned short flags, const char *keyfile, const char *password, const char *cafile, const char *capath, const char *randfile)
#endif
{ soap->keyfile = keyfile;
#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
  soap->keyid = keyid;
#endif
  soap->password = password;
  soap->cafile = cafile;
  soap->capath = capath;
  soap->ssl_flags = SOAP_SSL_CLIENT | flags;
#ifdef WITH_OPENSSL
  soap->dhfile = NULL;
  soap->randfile = randfile;
  if (!soap->fsslverify)
    soap->fsslverify = (flags & SOAP_SSL_ALLOW_EXPIRED_CERTIFICATE) == 0 ? ssl_verify_callback : ssl_verify_callback_allow_expired_certificate;
#endif
#ifdef WITH_GNUTLS
  (void)randfile;
  if (soap->session)
  { gnutls_deinit(soap->session);
    soap->session = NULL;
  }
  if (soap->xcred)
  { gnutls_certificate_free_credentials(soap->xcred);
    soap->xcred = NULL;
  }
#endif
#ifdef WITH_SYSTEMSSL
  (void)randfile;
  if (soap->ctx)
    gsk_environment_close(&soap->ctx);
#endif
  return soap->fsslauth(soap);
}
#endif
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_ssl_crl(struct soap *soap, const char *crlfile)
{
#ifdef WITH_OPENSSL
  if (crlfile && soap->ctx)
  {
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
    X509_STORE *store = SSL_CTX_get_cert_store(soap->ctx);
    if (*crlfile)
    { int ret;
      X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
      if ((ret = X509_load_crl_file(lookup, crlfile, X509_FILETYPE_PEM)) <= 0)
        return soap_set_receiver_error(soap, soap_ssl_error(soap, ret), "Can't read CRL file", SOAP_SSL_ERROR);
    }
    X509_VERIFY_PARAM *param = X509_VERIFY_PARAM_new();
    X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK);
    X509_STORE_set1_param(store, param);
    X509_VERIFY_PARAM_free(param);
#endif
  }
  else
    soap->crlfile = crlfile; /* activate later when store is available */
#endif
#ifdef WITH_GNUTLS
  if (crlfile && soap->xcred)
  { if (*crlfile)
      if (gnutls_certificate_set_x509_crl_file(soap->xcred, crlfile, GNUTLS_X509_FMT_PEM) < 0)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read CRL file", SOAP_SSL_ERROR);
  }
  else
    soap->crlfile = crlfile; /* activate later when xcred is available */
#endif
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_ssl_init()
{ /* Note: for MT systems, the main program MUST call soap_ssl_init() before any threads are started */
  if (!soap_ssl_init_done)
  { soap_ssl_init_done = 1;
#ifdef WITH_OPENSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms(); /* we keep ciphers and digests for the program's lifetime */
#ifndef WITH_LEAN
    SSL_load_error_strings();
#endif
    if (!RAND_load_file("/dev/urandom", 1024))
    { char buf[1024];
      RAND_seed(buf, sizeof(buf));
      while (!RAND_status())
      { int r = rand();
        RAND_seed(&r, sizeof(int));
      }
    }
#endif
#ifdef WITH_GNUTLS
# if defined(HAVE_PTHREAD_H)
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
# elif defined(HAVE_PTH_H)
    gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pth);
# endif
    gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);
    gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0); /* libgcrypt init done */
    gnutls_global_init();
#endif
  }
}
#endif
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
#ifndef PALM_1
SOAP_FMAC1
const char *
SOAP_FMAC2
soap_ssl_error(struct soap *soap, int ret)
{
#ifdef WITH_OPENSSL
  int err = SSL_get_error(soap->ssl, ret);
  const char *msg = soap_code_str(h_ssl_error_codes, err);
  if (msg)
    (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(msg) + 1), "%s\n", msg);
  else
    return ERR_error_string(err, soap->msgbuf);
  if (ERR_peek_error())
  { unsigned long r;
    while ((r = ERR_get_error()))
    { size_t l = strlen(soap->msgbuf);
      ERR_error_string_n(r, soap->msgbuf + l, sizeof(soap->msgbuf) - l);
    }
  }
  else
  { size_t l = strlen(soap->msgbuf);
    switch (ret)
    { case 0:
        soap_strcpy(soap->msgbuf + l, sizeof(soap->msgbuf) - l, "EOF was observed that violates the SSL/TLS protocol. The client probably provided invalid authentication information.");
        break;
      case -1:
        { const char *s = strerror(soap_errno);
          (SOAP_SNPRINTF(soap->msgbuf + l, sizeof(soap->msgbuf) - l, strlen(s) + 42), "Error observed by underlying SSL/TLS BIO: %s", s);
        }
        break;
    }
  }
  return soap->msgbuf;
#endif
#ifdef WITH_GNUTLS
  (void)soap;
  return gnutls_strerror(ret);
#endif
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_SYSTEMSSL
static int
ssl_recv(int sk, void *s, int n, char *user)
{
  (void)user;
  return recv(sk, s, n, 0);
}
#endif

/******************************************************************************/
#ifdef WITH_SYSTEMSSL
static int
ssl_send(int sk, void *s, int n, char *user)
{
  (void)user;
  return send(sk, s, n, 0);
}
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS) || defined(WITH_SYSTEMSSL)
#ifndef PALM_1
static int
ssl_auth_init(struct soap *soap)
{
#ifdef WITH_OPENSSL
  long flags;
  int mode;
#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
  EVP_PKEY* pkey;
#endif
  if (!soap_ssl_init_done)
    soap_ssl_init();
  ERR_clear_error();
  if (!soap->ctx)
  {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    /* TLS_method: a TLS/SSL connection established may understand the SSLv3, TLSv1, TLSv1.1 and TLSv1.2 protocols. */
    soap->ctx = SSL_CTX_new(TLS_method());
#else
    /* SSLv23_method: a TLS/SSL connection established may understand the SSLv3, TLSv1, TLSv1.1 and TLSv1.2 protocols. */
    soap->ctx = SSL_CTX_new(SSLv23_method());
#endif
    if (!soap->ctx)
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't setup context", SOAP_SSL_ERROR);
    /* The following alters the behavior of SSL read/write: */
#if 0
    SSL_CTX_set_mode(soap->ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY);
#endif
  }
  if (soap->randfile)
  { if (!RAND_load_file(soap->randfile, -1))
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't load randomness", SOAP_SSL_ERROR);
  }
  if (soap->cafile || soap->capath)
  { if (!SSL_CTX_load_verify_locations(soap->ctx, soap->cafile, soap->capath))
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read CA file", SOAP_SSL_ERROR);
    if (soap->cafile && (soap->ssl_flags & SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION))
      SSL_CTX_set_client_CA_list(soap->ctx, SSL_load_client_CA_file(soap->cafile));
  }
  if (!(soap->ssl_flags & SOAP_SSL_NO_DEFAULT_CA_PATH))
  { if (!SSL_CTX_set_default_verify_paths(soap->ctx))
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read default CA file and/or directory", SOAP_SSL_ERROR);
  }
  if (soap->crlfile)
  { if (soap_ssl_crl(soap, soap->crlfile))
      return soap->error;
  }
/* This code assumes a typical scenario, see alternative code below */
  if (soap->keyfile)
  { if (!SSL_CTX_use_certificate_chain_file(soap->ctx, soap->keyfile))
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read certificate key file", SOAP_SSL_ERROR);
    if (soap->password)
    { SSL_CTX_set_default_passwd_cb_userdata(soap->ctx, (void*)soap->password);
      SSL_CTX_set_default_passwd_cb(soap->ctx, ssl_password);
    }
    if (!SSL_CTX_use_PrivateKey_file(soap->ctx, soap->keyfile, SSL_FILETYPE_PEM))
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read key file", SOAP_SSL_ERROR);
#ifndef WM_SECURE_KEY_STORAGE
    if (!SSL_CTX_use_PrivateKey_file(soap->ctx, soap->keyfile, SSL_FILETYPE_PEM))
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read key file", SOAP_SSL_ERROR);
#endif
  }
#if defined(VXWORKS) && defined(WM_SECURE_KEY_STORAGE)
  if (NULL == (pkey = ipcom_key_db_pkey_get(soap->keyid)))
    return soap_set_receiver_error(soap, "SSL error", "Can't find key", SOAP_SSL_ERROR);
  if (0 == SSL_CTX_use_PrivateKey(soap->ctx, pkey))
    return soap_set_receiver_error(soap, "SSL error", "Can't read key", SOAP_SSL_ERROR);
#endif
/* Suggested alternative approach to check the key file for certs (cafile=NULL):*/
#if 0
  if (soap->password)
  { SSL_CTX_set_default_passwd_cb_userdata(soap->ctx, (void*)soap->password);
    SSL_CTX_set_default_passwd_cb(soap->ctx, ssl_password);
  }
  if (!soap->cafile || !SSL_CTX_use_certificate_chain_file(soap->ctx, soap->cafile))
  { if (soap->keyfile)
    { if (!SSL_CTX_use_certificate_chain_file(soap->ctx, soap->keyfile))
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read certificate or key file", SOAP_SSL_ERROR);
      if (!SSL_CTX_use_PrivateKey_file(soap->ctx, soap->keyfile, SSL_FILETYPE_PEM))
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read key file", SOAP_SSL_ERROR);
    }
  }
#endif
  if ((soap->ssl_flags & SOAP_SSL_RSA))
  { RSA *rsa = RSA_generate_key(SOAP_SSL_RSA_BITS, RSA_F4, NULL, NULL);
    if (!SSL_CTX_set_tmp_rsa(soap->ctx, rsa))
    { if (rsa)
        RSA_free(rsa);
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set RSA key", SOAP_SSL_ERROR);
    }
    RSA_free(rsa);
  }
  else if (soap->dhfile)
  { DH *dh = 0;
    char *s;
    int n = (int)soap_strtoul(soap->dhfile, &s, 10);
    /* if dhfile is numeric, treat it as a key length to generate DH params which can take a while */
    if (n >= 512 && s && *s == '\0')
#if defined(VXWORKS)
      DH_generate_parameters_ex(dh, n, 2/*or 5*/, NULL);
#else
      dh = DH_generate_parameters(n, 2/*or 5*/, NULL, NULL);
#endif
    else
    { BIO *bio;
      bio = BIO_new_file(soap->dhfile, "r");
      if (!bio)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read DH file", SOAP_SSL_ERROR);
      dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
      BIO_free(bio);
    }
    if (!dh || DH_check(dh, &n) != 1 || SSL_CTX_set_tmp_dh(soap->ctx, dh) < 0)
    { if (dh)
        DH_free(dh);
      return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set DH parameters", SOAP_SSL_ERROR);
    }
    DH_free(dh);
  }
  flags = (SSL_OP_ALL | SSL_OP_NO_SSLv2); /* disable SSL v2 by default */
  if ((soap->ssl_flags & SOAP_SSLv3))
  {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    flags |= SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
#else
    flags |= SSL_OP_NO_TLSv1;
#endif
  }
  else
  { if (!(soap->ssl_flags & SOAP_SSLv3_TLSv1))
      flags |= SSL_OP_NO_SSLv3; /* disable SSL v3 by default, unless SOAP_SSLv3 or SOAP_SSLv3_TLSv1 is set */
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    if ((soap->ssl_flags & SOAP_TLSv1_0))
      flags |= SSL_OP_NO_TLSv1_1 | SSL_OP_NO_TLSv1_2;
    else if ((soap->ssl_flags & SOAP_TLSv1_1))
      flags |= SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_2;
    else if ((soap->ssl_flags & SOAP_TLSv1_2))
      flags |= SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
#endif
  }
#ifdef SSL_OP_NO_TICKET
  /* TLS extension is enabled by default in OPENSSL v0.9.8k
     Disable it by adding SSL_OP_NO_TICKET */
  flags |= SSL_OP_NO_TICKET;
#endif
  SSL_CTX_set_options(soap->ctx, flags);
  if ((soap->ssl_flags & SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION))
    mode = (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
  else if ((soap->ssl_flags & SOAP_SSL_REQUIRE_SERVER_AUTHENTICATION))
    mode = SSL_VERIFY_PEER;
  else
    mode = SSL_VERIFY_NONE;
  SSL_CTX_set_verify(soap->ctx, mode, soap->fsslverify);
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  SSL_CTX_set_verify_depth(soap->ctx, 1);
#else
  SSL_CTX_set_verify_depth(soap->ctx, 9);
#endif
#endif
#ifdef WITH_GNUTLS
  int ret;
  if (!soap_ssl_init_done)
    soap_ssl_init();
  if (!soap->xcred)
  { gnutls_certificate_allocate_credentials(&soap->xcred);
    if (soap->cafile)
    { if (gnutls_certificate_set_x509_trust_file(soap->xcred, soap->cafile, GNUTLS_X509_FMT_PEM) < 0)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read CA file", SOAP_SSL_ERROR);
    }
    if (soap->crlfile)
    { if (soap_ssl_crl(soap, soap->crlfile))
        return soap->error;
    }
    if (soap->keyfile)
    { if (gnutls_certificate_set_x509_key_file(soap->xcred, soap->keyfile, soap->keyfile, GNUTLS_X509_FMT_PEM) < 0) /* Assumes that key and cert(s) are concatenated in the keyfile */
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't read key file", SOAP_SSL_ERROR);
    }
  }
  if ((soap->ssl_flags & SOAP_SSL_CLIENT))
  { gnutls_init(&soap->session, GNUTLS_CLIENT);
    if (soap->cafile || soap->crlfile || soap->keyfile)
    { ret = gnutls_priority_set_direct(soap->session, "PERFORMANCE", NULL);
      if (ret < 0)
        return soap_set_receiver_error(soap, soap_ssl_error(soap, ret), "SSL/TLS set priority error", SOAP_SSL_ERROR);
      gnutls_credentials_set(soap->session, GNUTLS_CRD_CERTIFICATE, soap->xcred);
    }
    else
    { if (!soap->acred)
        gnutls_anon_allocate_client_credentials(&soap->acred);
      gnutls_init(&soap->session, GNUTLS_CLIENT);
      gnutls_priority_set_direct(soap->session, "PERFORMANCE:+ANON-DH:!ARCFOUR-128", NULL);
      gnutls_credentials_set(soap->session, GNUTLS_CRD_ANON, soap->acred);
    }
  }
  else
  { if (!soap->keyfile)
      return soap_set_receiver_error(soap, "SSL/TLS error", "No key file: anonymous server authentication not supported in this release", SOAP_SSL_ERROR);
    if ((soap->ssl_flags & SOAP_SSL_RSA) && soap->rsa_params)
      gnutls_certificate_set_rsa_export_params(soap->xcred, soap->rsa_params);
    else if (soap->dh_params)
      gnutls_certificate_set_dh_params(soap->xcred, soap->dh_params);
    if (!soap->cache)
      gnutls_priority_init(&soap->cache, "NORMAL", NULL);
    gnutls_init(&soap->session, GNUTLS_SERVER);
    gnutls_priority_set(soap->session, soap->cache);
    gnutls_credentials_set(soap->session, GNUTLS_CRD_CERTIFICATE, soap->xcred);
    if ((soap->ssl_flags & SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION))
      gnutls_certificate_server_set_request(soap->session, GNUTLS_CERT_REQUEST);
    gnutls_session_enable_compatibility_mode(soap->session);
    if ((soap->ssl_flags & SOAP_SSLv3))
    { int protocol_priority[] = { GNUTLS_SSL3, 0 };
      if (gnutls_protocol_set_priority(soap->session, protocol_priority) != GNUTLS_E_SUCCESS)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set SSLv3 protocol", SOAP_SSL_ERROR);
    }
    else if ((soap->ssl_flags & SOAP_TLSv1_0))
    { int protocol_priority[] = { GNUTLS_TLS1_0, 0 };
      if (gnutls_protocol_set_priority(soap->session, protocol_priority) != GNUTLS_E_SUCCESS)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set TLSv1.0 protocol", SOAP_SSL_ERROR);
    }
    else if ((soap->ssl_flags & SOAP_TLSv1_1))
    { int protocol_priority[] = { GNUTLS_TLS1_1, 0 };
      if (gnutls_protocol_set_priority(soap->session, protocol_priority) != GNUTLS_E_SUCCESS)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set TLSv1.1 protocol", SOAP_SSL_ERROR);
    }
    else if ((soap->ssl_flags & SOAP_TLSv1_2))
    { int protocol_priority[] = { GNUTLS_TLS1_2, 0 };
      if (gnutls_protocol_set_priority(soap->session, protocol_priority) != GNUTLS_E_SUCCESS)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set TLSv1.2 protocol", SOAP_SSL_ERROR);
    }
    else if ((soap->ssl_flags & SOAP_SSLv3_TLSv1))
    { int protocol_priority[] = { GNUTLS_SSL3, GNUTLS_TLS1_0, GNUTLS_TLS1_1, GNUTLS_TLS1_2, 0 };
      if (gnutls_protocol_set_priority(soap->session, protocol_priority) != GNUTLS_E_SUCCESS)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set SSLv3 & TLSv1 protocols", SOAP_SSL_ERROR);
    }
    else
    { int protocol_priority[] = { GNUTLS_TLS1_0, GNUTLS_TLS1_1, GNUTLS_TLS1_2, 0 };
      if (gnutls_protocol_set_priority(soap->session, protocol_priority) != GNUTLS_E_SUCCESS)
        return soap_set_receiver_error(soap, "SSL/TLS error", "Can't set TLSv1 protocols", SOAP_SSL_ERROR);
    }
  }
#endif
#ifdef WITH_SYSTEMSSL
  if (!soap->ctx)
  { int err;
    err = gsk_environment_open(&soap->ctx);
    if (err == GSK_OK)
      err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_SSLV2, GSK_PROTOCOL_SSLV2_OFF); 
    if (err == GSK_OK)
    { if ((soap->ssl_flags & SOAP_SSLv3) || (soap->ssl_flags & SOAP_SSLv3_TLSv1))
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_SSLV3, GSK_PROTOCOL_SSLV3_ON);
      else
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_SSLV3, GSK_PROTOCOL_SSLV3_OFF);
    }
    if (!(soap->ssl_flags & SOAP_SSLv3))
    { if (err == GSK_OK)
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_TLSV1, GSK_PROTOCOL_TLSV1_ON);
      if (err == GSK_OK)
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_TLSV1_1, GSK_PROTOCOL_TLSV1_1_ON);
      if (err == GSK_OK)
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_TLSV1_2, GSK_PROTOCOL_TLSV1_2_ON); 
    }
    else
    { if (err == GSK_OK)
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_TLSV1, GSK_PROTOCOL_TLSV1_OFF);
      if (err == GSK_OK)
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_TLSV1_1, GSK_PROTOCOL_TLSV1_1_OFF);
      if (err == GSK_OK)
	err = gsk_attribute_set_enum(soap->ctx, GSK_PROTOCOL_TLSV1_2, GSK_PROTOCOL_TLSV1_2_OFF); 
    }
    if (err == GSK_OK)
      err = gsk_attribute_set_buffer(soap->ctx, GSK_KEYRING_FILE, soap->keyfile, 0); /* keyfile is a keyring .kdb file */
    if (err == GSK_OK)
      err = gsk_attribute_set_buffer(soap->ctx, GSK_KEYRING_PW, soap->password, 0); /* locked by password */
    if (err == GSK_OK)
      err = gsk_environment_init(soap->ctx);
    if (err != GSK_OK)
      return soap_set_receiver_error(soap, gsk_strerror(err), "SYSTEM SSL error in ssl_auth_init()", SOAP_SSL_ERROR);
  }
#endif
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_OPENSSL
#ifndef PALM_1
static int
ssl_password(char *buf, int num, int rwflag, void *userdata)
{ (void)rwflag;
  if (!userdata)
    return 0;
  soap_strcpy(buf, (size_t)num, (char*)userdata);
  return (int)strlen(buf);
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_OPENSSL
#ifndef PALM_1
static int
ssl_verify_callback(int ok, X509_STORE_CTX *store)
{ (void)store;
#ifdef SOAP_DEBUG
  if (!ok)
  { char buf[1024];
    int err = X509_STORE_CTX_get_error(store);
    X509 *cert = X509_STORE_CTX_get_current_cert(store);
    fprintf(stderr, "SSL verify error %d or warning with certificate at depth %d: %s\n", err, X509_STORE_CTX_get_error_depth(store), X509_verify_cert_error_string(err));
    X509_NAME_oneline(X509_get_issuer_name(cert), buf, sizeof(buf));
    fprintf(stderr, "certificate issuer %s\n", buf);
    X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
    fprintf(stderr, "certificate subject %s\n", buf);
    /* accept self-signed certificates and certificates out of date */
    switch (err)
    { case X509_V_ERR_CERT_NOT_YET_VALID:
      case X509_V_ERR_CERT_HAS_EXPIRED:
      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      case X509_V_ERR_UNABLE_TO_GET_CRL:
      case X509_V_ERR_CRL_NOT_YET_VALID:
      case X509_V_ERR_CRL_HAS_EXPIRED:
        X509_STORE_CTX_set_error(store, X509_V_OK);
        ok = 1;
	fprintf(stderr, "Initialize soap_ssl_client_context with SOAP_SSL_ALLOW_EXPIRED_CERTIFICATE to allow this verification error to pass without DEBUG mode enabled\n");
    }
  }
#endif
  /* Note: return 1 to try to continue, but unsafe progress will be terminated by OpenSSL */
  return ok;
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_OPENSSL
#ifndef PALM_1
static int
ssl_verify_callback_allow_expired_certificate(int ok, X509_STORE_CTX *store)
{ ok = ssl_verify_callback(ok, store);
  if (!ok)
  { /* accept self signed certificates, expired certificates, and certficiates w/o CRL */
    switch (X509_STORE_CTX_get_error(store))
    { case X509_V_ERR_CERT_NOT_YET_VALID:
      case X509_V_ERR_CERT_HAS_EXPIRED:
      case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      case X509_V_ERR_UNABLE_TO_GET_CRL:
      case X509_V_ERR_CRL_NOT_YET_VALID:
      case X509_V_ERR_CRL_HAS_EXPIRED:
        X509_STORE_CTX_set_error(store, X509_V_OK);
        ok = 1;
    }
  }
  /* Note: return 1 to continue, but unsafe progress will be terminated by SSL */
  return ok;
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_GNUTLS
static const char *
ssl_verify(struct soap *soap, const char *host)
{ unsigned int status;
  const char *err = NULL;
  int r = gnutls_certificate_verify_peers2(soap->session, &status);
  if (r < 0)
    err = "Certificate verify error";
  else if ((status & GNUTLS_CERT_INVALID))
    err = "The certificate is not trusted";
  else if ((status & GNUTLS_CERT_SIGNER_NOT_FOUND))
    err = "The certificate hasn't got a known issuer";
  else if ((status & GNUTLS_CERT_REVOKED))
    err = "The certificate has been revoked";
  else if (gnutls_certificate_type_get(soap->session) == GNUTLS_CRT_X509)
  { gnutls_x509_crt_t cert;
    const gnutls_datum_t *cert_list;
    unsigned int cert_list_size;
    if (gnutls_x509_crt_init(&cert) < 0)
      err = "Could not get X509 certificates";
    else if ((cert_list = gnutls_certificate_get_peers(soap->session, &cert_list_size)) == NULL)
      err = "Could not get X509 certificates";
    else if (gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
      err = "Error parsing X509 certificate";
    else if (!(soap->ssl_flags & SOAP_SSL_ALLOW_EXPIRED_CERTIFICATE) && gnutls_x509_crt_get_expiration_time(cert) < time(NULL))
      err = "The certificate has expired";
    else if (!(soap->ssl_flags & SOAP_SSL_ALLOW_EXPIRED_CERTIFICATE) && gnutls_x509_crt_get_activation_time(cert) > time(NULL))
      err = "The certificate is not yet activated";
    else if (host && !(soap->ssl_flags & SOAP_SSL_SKIP_HOST_CHECK))
    { if (!gnutls_x509_crt_check_hostname(cert, host))
        err = "Certificate host name mismatch";
    }
    gnutls_x509_crt_deinit(cert);
  }
  return err;
}
#endif

/******************************************************************************/
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
#ifndef WITH_NOIO
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_ssl_accept(struct soap *soap)
{ SOAP_SOCKET sk = soap->socket;
#ifdef WITH_OPENSSL
  BIO *bio;
  int retries, r, s;
  if (!soap_valid_socket(sk))
    return soap_set_receiver_error(soap, "SSL/TLS error", "No socket in soap_ssl_accept()", SOAP_SSL_ERROR);
  soap->ssl_flags &= ~SOAP_SSL_CLIENT;
  if (!soap->ctx && (soap->error = soap->fsslauth(soap)))
    return soap->error;
  if (!soap->ssl)
  { soap->ssl = SSL_new(soap->ctx);
    if (!soap->ssl)
      return soap_set_receiver_error(soap, "SSL/TLS error", "SSL_new() failed in soap_ssl_accept()", SOAP_SSL_ERROR);
  }
  else
    SSL_clear(soap->ssl);
  bio = BIO_new_socket((int)sk, BIO_NOCLOSE);
  SSL_set_bio(soap->ssl, bio, bio);
  /* Set SSL sockets to non-blocking */
  retries = 0;
  if (soap->accept_timeout)
  { SOAP_SOCKNONBLOCK(sk)
    retries = 10*soap->accept_timeout;
  }
  if (retries <= 0)
    retries = 100; /* timeout: 10 sec retries, 100 times 0.1 sec */
  while ((r = SSL_accept(soap->ssl)) <= 0)
  { int err;
    if (retries-- <= 0)
      break;
    err = SSL_get_error(soap->ssl, r);
    if (err == SSL_ERROR_WANT_ACCEPT || err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    { if (err == SSL_ERROR_WANT_READ)
        s = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, -100000);
      else
        s = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, -100000);
      if (s < 0)
        break;
    }
    else
    { soap->errnum = soap_socket_errno(sk);
      break;
    }
  }
  if (r <= 0)
  { soap_set_receiver_error(soap, soap_ssl_error(soap, r), "SSL_accept() failed in soap_ssl_accept()", SOAP_SSL_ERROR);
    soap_closesock(soap);
    return SOAP_SSL_ERROR;
  }
  if ((soap->ssl_flags & SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION))
  { X509 *peer;
    int err;
    if ((err = SSL_get_verify_result(soap->ssl)) != X509_V_OK)
    { soap_closesock(soap);
      return soap_set_sender_error(soap, X509_verify_cert_error_string(err), "SSL certificate presented by peer cannot be verified in soap_ssl_accept()", SOAP_SSL_ERROR);
    }
    peer = SSL_get_peer_certificate(soap->ssl);
    if (!peer)
    { soap_closesock(soap);
      return soap_set_sender_error(soap, "SSL/TLS error", "No SSL certificate was presented by the peer in soap_ssl_accept()", SOAP_SSL_ERROR);
    }
    X509_free(peer);
  }
#endif
#ifdef WITH_GNUTLS
  int retries = 0, r;
  if (!soap_valid_socket(sk))
    return soap_set_receiver_error(soap, "SSL/TLS error", "No socket in soap_ssl_accept()", SOAP_SSL_ERROR);
  soap->ssl_flags &= ~SOAP_SSL_CLIENT;
  if (!soap->session && (soap->error = soap->fsslauth(soap)))
  { soap_closesock(soap);
    return soap->error;
  }
  gnutls_transport_set_ptr(soap->session, (gnutls_transport_ptr_t)(long)sk);
  /* Set SSL sockets to non-blocking */
  if (soap->accept_timeout)
  { SOAP_SOCKNONBLOCK(sk)
    retries = 10*soap->accept_timeout;
  }
  if (retries <= 0)
    retries = 100; /* timeout: 10 sec retries, 100 times 0.1 sec */
  while ((r = gnutls_handshake(soap->session)))
  { int s;
    /* GNUTLS repeat handhake when GNUTLS_E_AGAIN */
    if (retries-- <= 0)
      break;
    if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED)
    { if (!gnutls_record_get_direction(soap->session))
        s = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, -100000);
      else
        s = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, -100000);
      if (s < 0)
        break;
    }
    else
    { soap->errnum = soap_socket_errno(sk);
      break;
    }
  }
  if (r)
  { soap_closesock(soap);
    return soap_set_receiver_error(soap, soap_ssl_error(soap, r), "SSL/TLS handshake failed", SOAP_SSL_ERROR);
  }
  if ((soap->ssl_flags & SOAP_SSL_REQUIRE_CLIENT_AUTHENTICATION))
  { const char *err = ssl_verify(soap, NULL);
    if (err)
    { soap_closesock(soap);
      return soap_set_receiver_error(soap, "SSL/TLS error", err, SOAP_SSL_ERROR);
    }
  }
#endif
#ifdef WITH_SYSTEMSSL
  gsk_iocallback local_io = { ssl_recv, ssl_send, NULL, NULL, NULL, NULL };
  int err, s;
  int retries = 0;
  if (soap->accept_timeout)
  { SOAP_SOCKNONBLOCK(sk)
    retries = 10*soap->accept_timeout;
  }
  if (retries <= 0)
    retries = 100; /* timeout: 10 sec retries, 100 times 0.1 sec */
  err = gsk_secure_socket_open(soap->ctx, &soap->ssl);
  if (err == GSK_OK)
    err = gsk_attribute_set_numeric_value(soap->ssl, GSK_FD, sk);
  if (err == GSK_OK)
    err = gsk_attribute_set_buffer(soap->ssl, GSK_KEYRING_LABEL, soap->cafile, 0);
  if (err == GSK_OK)
    err = gsk_attribute_set_enum(soap->ssl, GSK_SESSION_TYPE, GSK_SERVER_SESSION);
  if (err == GSK_OK)
    err = gsk_attribute_set_buffer(soap->ssl, GSK_V3_CIPHER_SPECS_EXPANDED, "0035002F000A", 0);
  if (err == GSK_OK)
    err = gsk_attribute_set_enum(soap->ssl, GSK_V3_CIPHERS, GSK_V3_CIPHERS_CHAR4);
  if (err == GSK_OK)
    err = gsk_attribute_set_callback(soap->ssl, GSK_IO_CALLBACK, &local_io);
  if (err != GSK_OK)
    return soap_set_receiver_error(soap, gsk_strerror(err), "SYSTEM SSL error in soap_ssl_accept()", SOAP_SSL_ERROR);
  while ((err = gsk_secure_socket_init(soap->ssl)) != GSK_OK)
  { if (retries-- <= 0)
      break;
    if (err == GSK_WOULD_BLOCK_READ || err == GSK_WOULD_BLOCK_WRITE)
    { if (err == GSK_WOULD_BLOCK_READ)
        s = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, -100000);
      else
        s = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, -100000);
      if (s < 0)
        break;
    }
    else
    { soap->errnum = soap_socket_errno(sk);
      break;
    }
  }
  if (err != GSK_OK)
    return soap_set_receiver_error(soap, gsk_strerror(err), "gsk_secure_socket_init() failed in soap_ssl_accept()", SOAP_SSL_ERROR);
#endif
  if (soap->recv_timeout || soap->send_timeout)
    SOAP_SOCKNONBLOCK(sk)
  else
    SOAP_SOCKBLOCK(sk)
  soap->imode |= SOAP_ENC_SSL;
  soap->omode |= SOAP_ENC_SSL;
  return SOAP_OK;
}
#endif
#endif
#endif

/******************************************************************************\
 *
 *	TCP/UDP [SSL/TLS] IPv4 and IPv6
 *
\******************************************************************************/

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static int
tcp_init(struct soap *soap)
{ soap->errmode = 1;
#ifdef WIN32
  if (tcp_done)
    return 0;
  else
  { WSADATA w;
    if (WSAStartup(MAKEWORD(1, 1), &w))
      return -1;
    tcp_done = 1;
  }
#endif
  return 0;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static const char*
tcp_error(struct soap *soap)
{ const char *msg = NULL;
  switch (soap->errmode)
  { case 0:
      msg = soap_strerror(soap);
      break;
    case 1:
      msg = "WSAStartup failed";
      break;
    case 2:
    {
#ifndef WITH_LEAN
      msg = soap_code_str(h_error_codes, soap->errnum);
      if (!msg)
#endif
      { (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), 37), "TCP/UDP IP error %d", soap->errnum);
        msg = soap->msgbuf;
      }
    }
  }
  return msg;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_IPV6
#ifndef WITH_NOIO
#ifndef PALM_1
static int
tcp_gethost(struct soap *soap, const char *addr, struct in_addr *inaddr)
{ soap_int32 iadd = -1;
  struct hostent hostent, *host = &hostent;
#ifdef VXWORKS
  int hostint;
  /* inet_addr(), and hostGetByName() expect "char *"; addr is a "const char *". */
  iadd = inet_addr((char*)addr);
#else
#if defined(_AIX43) || ((defined(TRU64) || defined(HP_UX)) && defined(HAVE_GETHOSTBYNAME_R))
  struct hostent_data ht_data;
#endif
#ifdef AS400
  iadd = inet_addr((void*)addr);
#else
  iadd = inet_addr((char*)addr);
#endif
#endif
  if (iadd != -1)
  { if (soap_memcpy((void*)inaddr, sizeof(struct in_addr), (const void*)&iadd, sizeof(iadd)))
      return soap->error = SOAP_EOM;
    return SOAP_OK;
  }
#if defined(__GLIBC__) || (defined(HAVE_GETHOSTBYNAME_R) && (defined(FREEBSD) || defined(__FreeBSD__))) || defined(__ANDROID__)
  if (gethostbyname_r(addr, &hostent, soap->buf, sizeof(soap->buf), &host, &soap->errnum) < 0)
    host = NULL;
#elif defined(_AIX43) || ((defined(TRU64) || defined(HP_UX)) && defined(HAVE_GETHOSTBYNAME_R))
  memset((void*)&ht_data, 0, sizeof(ht_data));
  if (gethostbyname_r(addr, &hostent, &ht_data) < 0)
  { host = NULL;
    soap->errnum = h_errno;
  }
#elif defined(HAVE_GETHOSTBYNAME_R)
  host = gethostbyname_r(addr, &hostent, soap->buf, sizeof(soap->buf), &soap->errnum);
#elif defined(VXWORKS)
  /* If the DNS resolver library resolvLib has been configured in the vxWorks
   * image, a query for the host IP address is sent to the DNS server, if the
   * name was not found in the local host table. */
  hostint = hostGetByName((char*)addr);
  if (hostint == ERROR)
  { host = NULL;
    soap->errnum = soap_errno;
  }
#else
#ifdef AS400
  if (!(host = gethostbyname((void*)addr)))
    soap->errnum = h_errno;
#else
  if (!(host = gethostbyname((char*)addr)))
    soap->errnum = h_errno;
#endif
#endif
  if (!host)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Host name not found\n"));
    return SOAP_ERR;
  }
#ifdef VXWORKS
  inaddr->s_addr = hostint;
#else
  if (soap_memcpy((void*)inaddr, sizeof(struct in_addr), (const void*)host->h_addr, (size_t)host->h_length))
    return soap->error = SOAP_EOM;
#endif
  return SOAP_OK;
}
#endif
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static SOAP_SOCKET
tcp_connect(struct soap *soap, const char *endpoint, const char *host, int port)
{
#ifdef WITH_IPV6
  struct addrinfo hints, *res, *ressave;
#endif
  SOAP_SOCKET sk;
  int err = 0;
#ifndef WITH_LEAN
#ifndef WIN32
  int len = sizeof(soap->buf);
#else
  int len = sizeof(soap->buf) + 1; /* speeds up windows xfer */
#endif
  int set = 1;
#endif
#if !defined(WITH_LEAN) || defined(WITH_OPENSSL) || defined(WITH_GNUTLS) || defined(WITH_SYSTEMSSL)
  int retries;
#endif
  if (soap_valid_socket(soap->socket))
    soap->fclosesocket(soap, soap->socket);
  soap->socket = SOAP_INVALID_SOCKET;
  if (tcp_init(soap))
  { soap->errnum = 0;
    soap_set_receiver_error(soap, tcp_error(soap), "TCP init failed in tcp_connect()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  soap->errmode = 0;
#ifdef WITH_IPV6
  memset((void*)&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    hints.ai_socktype = SOCK_DGRAM;
  else
#endif
    hints.ai_socktype = SOCK_STREAM;
  soap->errmode = 2;
  if (soap->proxy_host)
    err = getaddrinfo(soap->proxy_host, soap_int2s(soap, soap->proxy_port), &hints, &res);
  else
    err = getaddrinfo(host, soap_int2s(soap, port), &hints, &res);
  if (err || !res)
  { soap_set_receiver_error(soap, SOAP_GAI_STRERROR(err), "getaddrinfo failed in tcp_connect()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  ressave = res;
again:
  sk = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  soap->error = SOAP_OK;
  soap->errmode = 0;
#else
#ifndef WITH_LEAN
again:
#endif
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    sk = socket(AF_INET, SOCK_DGRAM, 0);
  else
#endif
    sk = socket(AF_INET, SOCK_STREAM, 0);
#endif
  if (!soap_valid_socket(sk))
  {
#ifdef WITH_IPV6
    if (res->ai_next)
    { res = res->ai_next;
      goto again;
    }
#endif
    soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "socket failed in tcp_connect()", SOAP_TCP_ERROR);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#ifdef SOCKET_CLOSE_ON_EXIT
#ifdef WIN32
#ifndef UNDER_CE
  SetHandleInformation((HANDLE)sk, HANDLE_FLAG_INHERIT, 0);
#endif
#else
  fcntl(sk, F_SETFD, 1);
#endif
#endif
#ifndef WITH_LEAN
  if ((soap->connect_flags & SO_LINGER))
  { struct linger linger;
    memset((void*)&linger, 0, sizeof(linger));
    linger.l_onoff = 1;
    linger.l_linger = soap->linger_time;
    if (setsockopt(sk, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(struct linger)))
    { soap->errnum = soap_socket_errno(sk);
      soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_LINGER failed in tcp_connect()", SOAP_TCP_ERROR);
      soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
      freeaddrinfo(ressave);
#endif
      return SOAP_INVALID_SOCKET;
    }
  }
  if ((soap->connect_flags & ~SO_LINGER) && setsockopt(sk, SOL_SOCKET, soap->connect_flags & ~SO_LINGER, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#ifndef UNDER_CE
  if ((soap->keep_alive || soap->tcp_keep_alive) && setsockopt(sk, SOL_SOCKET, SO_KEEPALIVE, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_KEEPALIVE failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
  if (setsockopt(sk, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_SNDBUF failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
  if (setsockopt(sk, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_RCVBUF failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#ifdef TCP_KEEPIDLE
  if (soap->tcp_keep_idle && setsockopt((SOAP_SOCKET)sk, IPPROTO_TCP, TCP_KEEPIDLE, (char*)&(soap->tcp_keep_idle), sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_KEEPIDLE failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, (SOAP_SOCKET)sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#endif
#ifdef TCP_KEEPINTVL
  if (soap->tcp_keep_intvl && setsockopt((SOAP_SOCKET)sk, IPPROTO_TCP, TCP_KEEPINTVL, (char*)&(soap->tcp_keep_intvl), sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_KEEPINTVL failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, (SOAP_SOCKET)sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#endif
#ifdef TCP_KEEPCNT
  if (soap->tcp_keep_cnt && setsockopt((SOAP_SOCKET)sk, IPPROTO_TCP, TCP_KEEPCNT, (char*)&(soap->tcp_keep_cnt), sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_KEEPCNT failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, (SOAP_SOCKET)sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#endif
#ifdef TCP_NODELAY
  if (!(soap->omode & SOAP_IO_UDP) && setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(sk);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_NODELAY failed in tcp_connect()", SOAP_TCP_ERROR);
    soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
    freeaddrinfo(ressave);
#endif
    return SOAP_INVALID_SOCKET;
  }
#endif
#ifdef WITH_IPV6
  if ((soap->omode & SOAP_IO_UDP) && soap->ipv6_multicast_if)
  { struct sockaddr_in6 *in6addr = (struct sockaddr_in6*)res->ai_addr;
    in6addr->sin6_scope_id = soap->ipv6_multicast_if;
  }
#endif
#ifdef IP_MULTICAST_TTL
  if ((soap->omode & SOAP_IO_UDP))
  { if (soap->ipv4_multicast_ttl)
    { unsigned char ttl = soap->ipv4_multicast_ttl;
      if (setsockopt(sk, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl)))
      { soap->errnum = soap_socket_errno(sk);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt IP_MULTICAST_TTL failed in tcp_connect()", SOAP_TCP_ERROR);
        soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
    }
    if ((soap->omode & SOAP_IO_UDP) && soap->ipv4_multicast_if && !soap->ipv6_multicast_if)
    { if (setsockopt(sk, IPPROTO_IP, IP_MULTICAST_IF, (char*)soap->ipv4_multicast_if, sizeof(struct in_addr)))
#ifndef WINDOWS
      { soap->errnum = soap_socket_errno(sk);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt IP_MULTICAST_IF failed in tcp_connect()", SOAP_TCP_ERROR);
        soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
#else
#ifndef IP_MULTICAST_IF
#define IP_MULTICAST_IF 2
#endif
      if (setsockopt(sk, IPPROTO_IP, IP_MULTICAST_IF, (char*)soap->ipv4_multicast_if, sizeof(struct in_addr)))
      { soap->errnum = soap_socket_errno(sk);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt IP_MULTICAST_IF failed in tcp_connect()", SOAP_TCP_ERROR);
        soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
#endif
    }
  }
#endif
#endif
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Opening socket=%d to host='%s' port=%d\n", sk, host, port));
#ifndef WITH_IPV6
  soap->peerlen = sizeof(soap->peer.in);
  memset((void*)&soap->peer.in, 0, sizeof(soap->peer.in));
  soap->peer.in.sin_family = AF_INET;
  soap->errmode = 2;
  if (soap->proxy_host)
  { if (soap->fresolve(soap, soap->proxy_host, &soap->peer.in.sin_addr))
    { soap_set_receiver_error(soap, tcp_error(soap), "get proxy host by name failed in tcp_connect()", SOAP_TCP_ERROR);
      soap->fclosesocket(soap, sk);
      return SOAP_INVALID_SOCKET;
    }
    soap->peer.in.sin_port = htons((short)soap->proxy_port);
  }
  else
  { if (soap->fresolve(soap, host, &soap->peer.in.sin_addr))
    { soap_set_receiver_error(soap, tcp_error(soap), "get host by name failed in tcp_connect()", SOAP_TCP_ERROR);
      soap->fclosesocket(soap, sk);
      return SOAP_INVALID_SOCKET;
    }
    soap->peer.in.sin_port = htons((short)port);
  }
  soap->errmode = 0;
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    return sk;
#endif
#else
  if ((soap->omode & SOAP_IO_UDP))
  { if (soap_memcpy((void*)&soap->peer.storage, sizeof(soap->peer.storage), (const void*)res->ai_addr, res->ai_addrlen))
    { soap->error = SOAP_EOM;
      soap->fclosesocket(soap, sk);
      sk = SOAP_INVALID_SOCKET;
    }
    soap->peerlen = res->ai_addrlen;
    freeaddrinfo(ressave);
    return sk;
  }
#endif
#ifndef WITH_LEAN
  if (soap->connect_timeout)
    SOAP_SOCKNONBLOCK(sk)
  else
    SOAP_SOCKBLOCK(sk)
  retries = 10;
#endif
  for (;;)
  {
#ifdef WITH_IPV6
    if (connect(sk, res->ai_addr, (int)res->ai_addrlen))
#else
    if (connect(sk, &soap->peer.addr, sizeof(soap->peer.in)))
#endif
    { err = soap_socket_errno(sk);
#ifdef WITH_IPV6
      if (err == SOAP_ECONNREFUSED && res->ai_next)
      { soap->fclosesocket(soap, sk);
        res = res->ai_next;
        goto again;
      }
#endif
#ifndef WITH_LEAN
      if (err == SOAP_EADDRINUSE)
      { soap->fclosesocket(soap, sk);
        if (retries-- > 0)
          goto again;
      }
      else if (soap->connect_timeout && (err == SOAP_EINPROGRESS || err == SOAP_EAGAIN || err == SOAP_EWOULDBLOCK))
      {
        SOAP_SOCKLEN_T k;
        for (;;)
        { int r;
          r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND, soap->connect_timeout);
          if (r > 0)
            break;
          if (!r)
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Connect timeout\n"));
            soap_set_receiver_error(soap, "Timeout", "connect failed in tcp_connect()", SOAP_TCP_ERROR);
            soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
	    if (res->ai_next)
	    { res = res->ai_next;
	      goto again;
	    }
            freeaddrinfo(ressave);
#endif
            return SOAP_INVALID_SOCKET;
          }
          r = soap->errnum = soap_socket_errno(sk);
          if (r != SOAP_EINTR)
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not connect to host\n"));
            soap_set_receiver_error(soap, tcp_error(soap), "connect failed in tcp_connect()", SOAP_TCP_ERROR);
            soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
	    if (res->ai_next)
	    { res = res->ai_next;
	      goto again;
	    }
            freeaddrinfo(ressave);
#endif
            return SOAP_INVALID_SOCKET;
          }
        }
        k = (SOAP_SOCKLEN_T)sizeof(soap->errnum);
        if (!getsockopt(sk, SOL_SOCKET, SO_ERROR, (char*)&soap->errnum, &k) && !soap->errnum)	/* portability note: see SOAP_SOCKLEN_T definition in stdsoap2.h */
          break;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not connect to host\n"));
        if (!soap->errnum)
          soap->errnum = soap_socket_errno(sk);
        soap_set_receiver_error(soap, tcp_error(soap), "connect failed in tcp_connect()", SOAP_TCP_ERROR);
        soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
	if (res->ai_next)
	{ res = res->ai_next;
	  goto again;
	}
        freeaddrinfo(ressave);
#endif
        return SOAP_INVALID_SOCKET;
      }
#endif
#ifdef WITH_IPV6
      if (res->ai_next)
      { res = res->ai_next;
        soap->fclosesocket(soap, sk);
        goto again;
      }
#endif
      if (err && err != SOAP_EINTR)
      { soap->errnum = err;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not connect to host\n"));
        soap_set_receiver_error(soap, tcp_error(soap), "connect failed in tcp_connect()", SOAP_TCP_ERROR);
        soap->fclosesocket(soap, sk);
#ifdef WITH_IPV6
        freeaddrinfo(ressave);
#endif
        return SOAP_INVALID_SOCKET;
      }
    }
    else
      break;
  }
#ifdef WITH_IPV6
  soap->peerlen = 0; /* IPv6: already connected so use send() */
  freeaddrinfo(ressave);
#endif
  soap->socket = sk;
  soap->imode &= ~SOAP_ENC_SSL;
  soap->omode &= ~SOAP_ENC_SSL;
  if (!soap_tag_cmp(endpoint, "https:*"))
  {
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS) || defined(WITH_SYSTEMSSL)
#ifdef WITH_OPENSSL
    BIO *bio;
#endif
#ifdef WITH_SYSTEMSSL
    gsk_iocallback local_io = { ssl_recv, ssl_send, NULL, NULL, NULL, NULL };
#endif
    int r;
    if (soap->proxy_host)
    { soap_mode m = soap->mode; /* preserve settings */
      soap_mode om = soap->omode; /* make sure we only parse HTTP */
      size_t n = soap->count; /* save the content length */
      const char *userid, *passwd;
      int status = soap->status; /* save the current status/command */
      short keep_alive = soap->keep_alive; /* save the KA status */
      soap->omode &= ~SOAP_ENC; /* mask IO and ENC */
      soap->omode |= SOAP_IO_BUFFER;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Connecting to %s proxy server %s for destination endpoint %s\n", soap->proxy_http_version, soap->proxy_host, endpoint));
#ifdef WITH_NTLM
      if (soap->ntlm_challenge)
      { if (soap_ntlm_handshake(soap, SOAP_CONNECT, endpoint, host, port))
          return soap->error;
      }
#endif
      if (soap_begin_send(soap))
      { soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
      soap->status = SOAP_CONNECT;
      soap->keep_alive = 1;
      if ((soap->error = soap->fpost(soap, endpoint, host, port, NULL, NULL, 0))
       || soap_end_send_flush(soap))
      { soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
      soap->keep_alive = keep_alive;
      soap->omode = om;
      om = soap->imode;
      soap->imode &= ~SOAP_ENC; /* mask IO and ENC */
      userid = soap->userid; /* preserve */
      passwd = soap->passwd; /* preserve */
      if ((soap->error = soap->fparse(soap)))
      { soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
      soap->status = status; /* restore */
      soap->userid = userid; /* restore */
      soap->passwd = passwd; /* restore */
      soap->imode = om; /* restore */
      soap->count = n; /* restore */
      if (soap_begin_send(soap))
      { soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
      if (endpoint)
        soap_strcpy(soap->endpoint, sizeof(soap->endpoint), endpoint); /* restore */
      soap->mode = m;
    }
#ifdef WITH_OPENSSL
    soap->ssl_flags |= SOAP_SSL_CLIENT;
    if (!soap->ctx && (soap->error = soap->fsslauth(soap)))
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "SSL required, but no ctx set\n"));
      soap->fclosesocket(soap, sk);
      soap->error = SOAP_SSL_ERROR;
      return SOAP_INVALID_SOCKET;
    }
    if (!soap->ssl)
    { soap->ssl = SSL_new(soap->ctx);
      if (!soap->ssl)
      { soap->fclosesocket(soap, sk);
        soap->error = SOAP_SSL_ERROR;
        return SOAP_INVALID_SOCKET;
      }
    }
    else
      SSL_clear(soap->ssl);
    if (soap->session)
    { if (!strcmp(soap->session_host, host) && soap->session_port == port)
        SSL_set_session(soap->ssl, soap->session);
      SSL_SESSION_free(soap->session);
      soap->session = NULL;
    }
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
    if (!(soap->ssl_flags & SOAP_SSLv3) && !SSL_set_tlsext_host_name(soap->ssl, host))
    { soap_set_receiver_error(soap, "SSL/TLS error", "SNI failed", SOAP_SSL_ERROR);
      soap->fclosesocket(soap, sk);
      return SOAP_INVALID_SOCKET;
    }
#endif
    bio = BIO_new_socket((int)sk, BIO_NOCLOSE);
    SSL_set_bio(soap->ssl, bio, bio);
    /* Connect timeout: set SSL sockets to non-blocking */
    retries = 0;
    if (soap->connect_timeout)
    { SOAP_SOCKNONBLOCK(sk)
      retries = 10*soap->connect_timeout;
    }
    else
      SOAP_SOCKBLOCK(sk)
    if (retries <= 0)
      retries = 100; /* timeout: 10 sec retries, 100 times 0.1 sec */
    /* Try connecting until success or timeout (when nonblocking) */
    do
    { if ((r = SSL_connect(soap->ssl)) <= 0)
      { int err = SSL_get_error(soap->ssl, r);
        if (err == SSL_ERROR_WANT_CONNECT || err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        { int s;
          if (err == SSL_ERROR_WANT_READ)
            s = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, -100000);
          else
            s = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, -100000);
          if (s < 0)
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "SSL_connect/select error in tcp_connect\n"));
            soap_set_receiver_error(soap, soap_ssl_error(soap, r), "SSL_connect failed in tcp_connect()", SOAP_TCP_ERROR);
            soap->fclosesocket(soap, sk);
            return SOAP_INVALID_SOCKET;
          }
          if (s == 0 && retries-- <= 0)
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "SSL/TLS connect timeout\n"));
            soap_set_receiver_error(soap, "Timeout", "SSL_connect failed in tcp_connect()", SOAP_TCP_ERROR);
            soap->fclosesocket(soap, sk);
            return SOAP_INVALID_SOCKET;
          }
        }
        else
        { soap_set_receiver_error(soap, soap_ssl_error(soap, r), "SSL_connect error in tcp_connect()", SOAP_SSL_ERROR);
          soap->fclosesocket(soap, sk);
          return SOAP_INVALID_SOCKET;
        }
      }
    } while (!SSL_is_init_finished(soap->ssl));
    /* Set SSL sockets to nonblocking */
    SOAP_SOCKNONBLOCK(sk)
    /* Check server credentials when required */
    if ((soap->ssl_flags & SOAP_SSL_REQUIRE_SERVER_AUTHENTICATION))
    { int err;
      if ((err = SSL_get_verify_result(soap->ssl)) != X509_V_OK)
      { soap_set_sender_error(soap, X509_verify_cert_error_string(err), "SSL/TLS certificate presented by peer cannot be verified in tcp_connect()", SOAP_SSL_ERROR);
        soap->fclosesocket(soap, sk);
        return SOAP_INVALID_SOCKET;
      }
      if (!(soap->ssl_flags & SOAP_SSL_SKIP_HOST_CHECK))
      { X509_NAME *subj;
        STACK_OF(CONF_VALUE) *val = NULL;
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
        GENERAL_NAMES *names = NULL;
#else
        int ext_count;
#endif
        int ok = 0;
        X509 *peer = SSL_get_peer_certificate(soap->ssl);
        if (!peer)
        { soap_set_sender_error(soap, "SSL/TLS error", "No SSL/TLS certificate was presented by the peer in tcp_connect()", SOAP_SSL_ERROR);
          soap->fclosesocket(soap, sk);
          return SOAP_INVALID_SOCKET;
        }
#if (OPENSSL_VERSION_NUMBER < 0x0090800fL)
        ext_count = X509_get_ext_count(peer);
        if (ext_count > 0)
        { int i;
          for (i = 0; i < ext_count; i++)
          { X509_EXTENSION *ext = X509_get_ext(peer, i);
            const char *ext_str = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));
            if (ext_str && !strcmp(ext_str, "subjectAltName"))
            { X509V3_EXT_METHOD *meth = (X509V3_EXT_METHOD*)X509V3_EXT_get(ext);
              unsigned char *data;
              if (!meth)
                break;
              data = ext->value->data;
              if (data)
              {
#if (OPENSSL_VERSION_NUMBER > 0x00907000L)
                void *ext_data;
                if (meth->it)
                  ext_data = ASN1_item_d2i(NULL, &data, ext->value->length, ASN1_ITEM_ptr(meth->it));
                else
		{
#if (OPENSSL_VERSION_NUMBER > 0x0090800fL)
                  ext_data = meth->d2i(NULL, (const unsigned char **)&data, ext->value->length);
#else
                  ext_data = meth->d2i(NULL, &data, ext->value->length);
#endif
		}
                if (ext_data)
                  val = meth->i2v(meth, ext_data, NULL);
        	else
        	  val = NULL;
                if (meth->it)
                  ASN1_item_free((ASN1_VALUE*)ext_data, ASN1_ITEM_ptr(meth->it));
                else
                  meth->ext_free(ext_data);
#else
                void *ext_data = meth->d2i(NULL, &data, ext->value->length);
                if (ext_data)
                  val = meth->i2v(meth, ext_data, NULL);
                meth->ext_free(ext_data);
#endif
        	if (val)
                { int j;
                  for (j = 0; j < sk_CONF_VALUE_num(val); j++)
                  { CONF_VALUE *nval = sk_CONF_VALUE_value(val, j);
                    if (nval && !strcmp(nval->name, "DNS") && !strcmp(nval->value, host))
                    { ok = 1;
                      break;
                    }
                  }
                  sk_CONF_VALUE_pop_free(val, X509V3_conf_free);
                }
              }
            }
            if (ok)
              break;
          }
        }
#else
        names = (GENERAL_NAMES*)X509_get_ext_d2i(peer, NID_subject_alt_name, NULL, NULL);
        if (names)
        { val = i2v_GENERAL_NAMES(NULL, names, val);
          sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
        }
        if (val)
        { int j;
          for (j = 0; j < sk_CONF_VALUE_num(val); j++)
          { CONF_VALUE *nval = sk_CONF_VALUE_value(val, j);
            if (nval && !strcmp(nval->name, "DNS") && !strcmp(nval->value, host))
            { ok = 1;
              break;
            }
          }
          sk_CONF_VALUE_pop_free(val, X509V3_conf_free);
        }
#endif
        if (!ok && (subj = X509_get_subject_name(peer)))
        { int i = -1;
          do
          { ASN1_STRING *name;
            i = X509_NAME_get_index_by_NID(subj, NID_commonName, i);
            if (i == -1)
              break;
            name = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subj, i));
            if (name)
            { if (!soap_tag_cmp(host, (const char*)M_ASN1_STRING_data(name)))
                ok = 1;
              else
              { unsigned char *tmp = NULL;
                ASN1_STRING_to_UTF8(&tmp, name);
                if (tmp)
                { if (!soap_tag_cmp(host, (const char*)tmp))
                    ok = 1;
                  else if (tmp[0] == '*') /* wildcard domain */
                  { const char *t = strchr(host, '.');
                    if (t && !soap_tag_cmp(t, (const char*)tmp+1))
                      ok = 1;
                  }
                  OPENSSL_free(tmp);
                }
              }
            }
          } while (!ok);
        }
        X509_free(peer);
        if (!ok)
        { soap_set_sender_error(soap, "SSL/TLS error", "SSL/TLS certificate host name mismatch in tcp_connect()", SOAP_SSL_ERROR);
          soap->fclosesocket(soap, sk);
          return SOAP_INVALID_SOCKET;
        }
      }
    }
#endif
#ifdef WITH_GNUTLS
    soap->ssl_flags |= SOAP_SSL_CLIENT;
    if (!soap->session && (soap->error = soap->fsslauth(soap)))
    { soap->fclosesocket(soap, sk);
      return SOAP_INVALID_SOCKET;
    }
    gnutls_transport_set_ptr(soap->session, (gnutls_transport_ptr_t)(long)sk);
    /* Set SSL sockets to non-blocking */
    if (soap->connect_timeout)
    { SOAP_SOCKNONBLOCK(sk)
      retries = 10*soap->connect_timeout;
    }
    else
      SOAP_SOCKBLOCK(sk)
    if (retries <= 0)
      retries = 100; /* timeout: 10 sec retries, 100 times 0.1 sec */
    while ((r = gnutls_handshake(soap->session)))
    { int s;
      /* GNUTLS repeat handhake when GNUTLS_E_AGAIN */
      if (retries-- <= 0)
        break;
      if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED)
      { if (!gnutls_record_get_direction(soap->session))
          s = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, -100000);
        else
          s = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, -100000);
        if (s < 0)
          break;
      }
      else
      { soap->errnum = soap_socket_errno(sk);
        break;
      }
    }
    if (r)
    { soap_set_sender_error(soap, soap_ssl_error(soap, r), "SSL/TLS handshake failed", SOAP_SSL_ERROR);
      soap->fclosesocket(soap, sk);
      return SOAP_INVALID_SOCKET;
    }
    if ((soap->ssl_flags & SOAP_SSL_REQUIRE_SERVER_AUTHENTICATION))
    { const char *err = ssl_verify(soap, host);
      if (err)
      { soap->fclosesocket(soap, sk);
        soap->error = soap_set_sender_error(soap, "SSL/TLS verify error", err, SOAP_SSL_ERROR);
        return SOAP_INVALID_SOCKET;
      }
    }
#endif
#ifdef WITH_SYSTEMSSL
    soap->ssl_flags |= SOAP_SSL_CLIENT;
    if (!soap->ctx && (soap->error = soap->fsslauth(soap)))
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "SSL required, but no ctx set\n"));
      soap->fclosesocket(soap, sk);
      soap->error = SOAP_SSL_ERROR;
      return SOAP_INVALID_SOCKET;
    }
    /* Connect timeout: set SSL sockets to non-blocking */
    retries = 0;
    if (soap->connect_timeout)
    { SOAP_SOCKNONBLOCK(sk)
      retries = 10*soap->connect_timeout;
    }
    else
      SOAP_SOCKBLOCK(sk)
    if (retries <= 0)
      retries = 100; /* timeout: 10 sec retries, 100 times 0.1 sec */
    err = gsk_secure_socket_open(soap->ctx, &soap->ssl);
    if (err == GSK_OK)
      err = gsk_attribute_set_numeric_value(soap->ssl, GSK_FD, sk);
    if (err == GSK_OK)
      err = gsk_attribute_set_buffer(soap->ssl, GSK_KEYRING_LABEL, soap->cafile, 0); /* Certificate label */
    if (err == GSK_OK)
      err = gsk_attribute_set_enum(soap->ssl, GSK_SESSION_TYPE, GSK_CLIENT_SESSION);
    if (err == GSK_OK)
      err = gsk_attribute_set_buffer(soap->ssl, GSK_V3_CIPHER_SPECS_EXPANDED, "0035002F000A", 0);
    if (err == GSK_OK)
      err = gsk_attribute_set_enum(soap->ssl, GSK_V3_CIPHERS, GSK_V3_CIPHERS_CHAR4);
    if (err == GSK_OK)
      err = gsk_attribute_set_callback(soap->ssl, GSK_IO_CALLBACK, &local_io);
    if (err != GSK_OK)
    { soap_set_receiver_error(soap, gsk_strerror(err), "SYSTEM SSL error in tcp_connect()", SOAP_SSL_ERROR);
      return SOAP_INVALID_SOCKET;
    }
    /* Try connecting until success or timeout (when nonblocking) */
    while ((err = gsk_secure_socket_init(soap->ssl)) != GSK_OK)
    { if (err == GSK_WOULD_BLOCK_READ || err == GSK_WOULD_BLOCK_WRITE)
      { if (err == GSK_WOULD_BLOCK_READ)
	  r = tcp_select(soap, sk, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, -100000);
	else
	  r = tcp_select(soap, sk, SOAP_TCP_SELECT_SND | SOAP_TCP_SELECT_ERR, -100000);
	if (r < 0)
	{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "SSL_connect/select error in tcp_connect\n"));
	  soap_set_receiver_error(soap, gsk_strerror(err), "gsk_secure_socket_init failed in tcp_connect()", SOAP_TCP_ERROR);
	  soap->fclosesocket(soap, sk);
	  return SOAP_INVALID_SOCKET;
	}
	if (r == 0 && retries-- <= 0)
	{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "SSL/TLS connect timeout\n"));
	  soap_set_receiver_error(soap, "Timeout", "in tcp_connect()", SOAP_TCP_ERROR);
	  soap->fclosesocket(soap, sk);
	  return SOAP_INVALID_SOCKET;
	}
      }
      else
      { soap_set_receiver_error(soap, gsk_strerror(err), "gsk_secure_socket_init() failed in tcp_connect()", SOAP_SSL_ERROR);
	soap->fclosesocket(soap, sk);
	return SOAP_INVALID_SOCKET;
      }
    }
#endif
    soap->imode |= SOAP_ENC_SSL;
    soap->omode |= SOAP_ENC_SSL;
#else
    soap->fclosesocket(soap, sk);
    soap->error = SOAP_SSL_ERROR;
    return SOAP_INVALID_SOCKET;
#endif
  }
  if (soap->recv_timeout || soap->send_timeout)
    SOAP_SOCKNONBLOCK(sk)
  else
    SOAP_SOCKBLOCK(sk)
  return sk;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static int
tcp_select(struct soap *soap, SOAP_SOCKET sk, int flags, int timeout)
{ int r;
  struct timeval tv;
  fd_set fd[3], *rfd, *sfd, *efd;
  int retries = 0;
  int eintr = SOAP_MAXEINTR;
  soap->errnum = 0;
#ifndef WIN32
#if !defined(FD_SETSIZE) || defined(__QNX__) || defined(QNX)
  /* no FD_SETSIZE or select() is not MT safe on some QNX: always poll */
  if (1)
#else
  /* if fd max set size exceeded, use poll() */
  if ((int)sk >= (int)FD_SETSIZE)
#endif
#ifdef HAVE_POLL
  { struct pollfd pollfd;
    pollfd.fd = (int)sk;
    pollfd.events = 0;
    if (flags & SOAP_TCP_SELECT_RCV)
      pollfd.events |= POLLIN;
    if (flags & SOAP_TCP_SELECT_SND)
      pollfd.events |= POLLOUT;
    if (flags & SOAP_TCP_SELECT_ERR)
      pollfd.events |= POLLERR;
    if (timeout <= 0)
      timeout /= -1000; /* -usec -> ms */
    else
    { retries = timeout - 1;
      timeout = 1000;
    }
    do
    { r = poll(&pollfd, 1, timeout);
      if (r < 0 && (soap->errnum = soap_socket_errno(sk)) == SOAP_EINTR && eintr-- > 0)
        r = 0;
      else if (retries-- <= 0)
        break;
    } while (r == 0);
    if (r > 0)
    { r = 0;
      if ((flags & SOAP_TCP_SELECT_RCV) && (pollfd.revents & POLLIN))
        r |= SOAP_TCP_SELECT_RCV;
      if ((flags & SOAP_TCP_SELECT_SND) && (pollfd.revents & POLLOUT))
        r |= SOAP_TCP_SELECT_SND;
      if ((flags & SOAP_TCP_SELECT_ERR) && (pollfd.revents & POLLERR))
        r |= SOAP_TCP_SELECT_ERR;
    }
    else if (r == 0)
      soap->errnum = 0;
    return r;
  }
#else
  { soap->error = SOAP_FD_EXCEEDED;
    return -1;
  }
#endif
#endif
  if (timeout > 0)
    retries = timeout - 1;
  do
  { rfd = sfd = efd = NULL;
    if (flags & SOAP_TCP_SELECT_RCV)
    { rfd = &fd[0];
      FD_ZERO(rfd);
      FD_SET(sk, rfd);
    }
    if (flags & SOAP_TCP_SELECT_SND)
    { sfd = &fd[1];
      FD_ZERO(sfd);
      FD_SET(sk, sfd);
    }
    if (flags & SOAP_TCP_SELECT_ERR)
    { efd = &fd[2];
      FD_ZERO(efd);
      FD_SET(sk, efd);
    }
    if (timeout <= 0)
    { tv.tv_sec = -timeout / 1000000;
      tv.tv_usec = -timeout % 1000000;
    }
    else
    { tv.tv_sec = 1;
      tv.tv_usec = 0;
    }
    r = select((int)sk + 1, rfd, sfd, efd, &tv);
    if (r < 0 && (soap->errnum = soap_socket_errno(sk)) == SOAP_EINTR && eintr-- > 0)
      r = 0;
    else if (retries-- <= 0)
      break;
  } while (r == 0);
  if (r > 0)
  { r = 0;
    if ((flags & SOAP_TCP_SELECT_RCV) && FD_ISSET(sk, rfd))
      r |= SOAP_TCP_SELECT_RCV;
    if ((flags & SOAP_TCP_SELECT_SND) && FD_ISSET(sk, sfd))
      r |= SOAP_TCP_SELECT_SND;
    if ((flags & SOAP_TCP_SELECT_ERR) && FD_ISSET(sk, efd))
      r |= SOAP_TCP_SELECT_ERR;
  }
  else if (r == 0)
    soap->errnum = 0;
  return r;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static SOAP_SOCKET
tcp_accept(struct soap *soap, SOAP_SOCKET s, struct sockaddr *a, int *n)
{ SOAP_SOCKET sk;
  (void)soap;
  sk = accept(s, a, (SOAP_SOCKLEN_T*)n); /* portability note: see SOAP_SOCKLEN_T definition in stdsoap2.h */
#ifdef SOCKET_CLOSE_ON_EXIT
#ifdef WIN32
#ifndef UNDER_CE
  SetHandleInformation((HANDLE)sk, HANDLE_FLAG_INHERIT, 0);
#endif
#else
  fcntl(sk, F_SETFD, FD_CLOEXEC);
#endif
#endif
  return sk;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static int
tcp_disconnect(struct soap *soap)
{
#ifdef WITH_OPENSSL
  if (soap->ssl)
  { int r, s = 0;
    if (soap->session)
    { SSL_SESSION_free(soap->session);
      soap->session = NULL;
    }
    if (*soap->host)
    { soap->session = SSL_get1_session(soap->ssl);
      if (soap->session)
      { soap_strcpy(soap->session_host, sizeof(soap->session_host), soap->host);
        soap->session_port = soap->port;
      }
    }
    r = SSL_shutdown(soap->ssl);
    /* SSL shutdown does not work when reads are pending, non-blocking */
    if (r == 0)
    { while (SSL_want_read(soap->ssl))
      { if (SSL_read(soap->ssl, NULL, 0)
         || soap_socket_errno(soap->socket) != SOAP_EAGAIN)
        { r = SSL_shutdown(soap->ssl);
          break;
        }
      }
    }
    if (r == 0)
    { if (soap_valid_socket(soap->socket))
      { if (!soap->fshutdownsocket(soap, soap->socket, SOAP_SHUT_WR))
        {
#if !defined(WITH_LEAN) && !defined(WIN32)
          /*
          wait up to 5 seconds for close_notify to be sent by peer (if peer not
          present, this avoids calling SSL_shutdown() which has a lengthy return
          timeout)
          */
          r = tcp_select(soap, soap->socket, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_ERR, 5);
          if (r <= 0)
          { soap->errnum = 0;
            DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Connection lost...\n"));
            soap->fclosesocket(soap, soap->socket);
            soap->socket = SOAP_INVALID_SOCKET;
            ERR_remove_state(0);
            SSL_free(soap->ssl);
            soap->ssl = NULL;
            return SOAP_OK;
          }
#else
          r = SSL_shutdown(soap->ssl);
#endif
        }
      }
    }
    if (r != 1)
    { s = ERR_get_error();
      if (s)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Shutdown failed: %d\n", SSL_get_error(soap->ssl, r)));
        if (soap_valid_socket(soap->socket) && !(soap->omode & SOAP_IO_UDP))
        { soap->fclosesocket(soap, soap->socket);
          soap->socket = SOAP_INVALID_SOCKET;
        }
      }
    }
    SSL_free(soap->ssl);
    soap->ssl = NULL;
    if (s)
      return SOAP_SSL_ERROR;
    ERR_remove_state(0);
  }
#endif
#ifdef WITH_GNUTLS
  if (soap->session)
  { gnutls_bye(soap->session, GNUTLS_SHUT_RDWR);
    gnutls_deinit(soap->session);
    soap->session = NULL;
  }
#endif
#ifdef WITH_SYSTEMSSL
  if (soap->ssl)
  { gsk_secure_socket_shutdown(soap->ssl);
    gsk_secure_socket_close(&soap->ssl);
  }
#endif
  if (soap_valid_socket(soap->socket) && !(soap->omode & SOAP_IO_UDP))
  { soap->fshutdownsocket(soap, soap->socket, SOAP_SHUT_RDWR);
    soap->fclosesocket(soap, soap->socket);
    soap->socket = SOAP_INVALID_SOCKET;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static int
tcp_closesocket(struct soap *soap, SOAP_SOCKET sk)
{ (void)soap;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Close socket=%d\n", (int)sk));
  return soap_closesocket(sk);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static int
tcp_shutdownsocket(struct soap *soap, SOAP_SOCKET sk, int how)
{ (void)soap;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Shutdown socket=%d how=%d\n", (int)sk, how));
  return shutdown(sk, how);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
SOAP_FMAC1
SOAP_SOCKET
SOAP_FMAC2
soap_bind(struct soap *soap, const char *host, int port, int backlog)
{
#ifdef WITH_IPV6
  struct addrinfo *addrinfo = NULL;
  struct addrinfo hints;
  struct addrinfo res;
  int err;
#ifdef WITH_NO_IPV6_V6ONLY
  int unset = 0;
#endif
#endif
#ifndef WITH_LEAN
#ifndef WIN32
  int len = sizeof(soap->buf);
#else
  int len = sizeof(soap->buf) + 1; /* speeds up windows xfer */
#endif
  int set = 1;
#endif
  if (soap_valid_socket(soap->master))
  { soap->fclosesocket(soap, soap->master);
    soap->master = SOAP_INVALID_SOCKET;
  }
  soap->socket = SOAP_INVALID_SOCKET;
  soap->errmode = 1;
  if (tcp_init(soap))
  { soap_set_receiver_error(soap, tcp_error(soap), "TCP init failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#ifdef WITH_IPV6
  memset((void*)&hints, 0, sizeof(hints));
  hints.ai_family = PF_INET6;
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    hints.ai_socktype = SOCK_DGRAM;
  else
#endif
    hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  soap->errmode = 2;
  err = getaddrinfo(host, soap_int2s(soap, port), &hints, &addrinfo);
  if (err || !addrinfo)
  { soap_set_receiver_error(soap, SOAP_GAI_STRERROR(err), "getaddrinfo failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  res = *addrinfo;
  if (soap_memcpy((void*)&soap->peer.storage, sizeof(soap->peer.storage), (const void*)addrinfo->ai_addr, addrinfo->ai_addrlen))
  { soap->error = SOAP_EOM;
    return SOAP_INVALID_SOCKET;
  }
  soap->peerlen = addrinfo->ai_addrlen;
  res.ai_addr = &soap->peer.addr;
  res.ai_addrlen = soap->peerlen;
  freeaddrinfo(addrinfo);
  soap->master = (int)socket(res.ai_family, res.ai_socktype, res.ai_protocol);
#else
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    soap->master = (int)socket(AF_INET, SOCK_DGRAM, 0);
  else
#endif
    soap->master = (int)socket(AF_INET, SOCK_STREAM, 0);
#endif
  soap->errmode = 0;
  if (!soap_valid_socket(soap->master))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "socket failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  soap->port = port;
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    soap->socket = soap->master;
#endif
#ifdef SOCKET_CLOSE_ON_EXIT
#ifdef WIN32
#ifndef UNDER_CE
  SetHandleInformation((HANDLE)soap->master, HANDLE_FLAG_INHERIT, 0);
#endif
#else
  fcntl(soap->master, F_SETFD, 1);
#endif
#endif
#ifndef WITH_LEAN
  if (soap->bind_flags && setsockopt(soap->master, SOL_SOCKET, soap->bind_flags, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#ifndef UNDER_CE
  if (((soap->imode | soap->omode) & SOAP_IO_KEEPALIVE) && (!((soap->imode | soap->omode) & SOAP_IO_UDP)) && setsockopt(soap->master, SOL_SOCKET, SO_KEEPALIVE, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_KEEPALIVE failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  if (setsockopt(soap->master, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_SNDBUF failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  if (setsockopt(soap->master, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_RCVBUF failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#ifdef TCP_NODELAY
  if (!(soap->omode & SOAP_IO_UDP) && setsockopt(soap->master, IPPROTO_TCP, TCP_NODELAY, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_NODELAY failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#endif
#ifdef TCP_FASTOPEN
  if (!(soap->omode & SOAP_IO_UDP) && setsockopt(soap->master, SOL_TCP, TCP_FASTOPEN, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_FASTOPEN failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#endif
#endif
#endif
#ifdef WITH_IPV6
#ifdef WITH_IPV6_V6ONLY
  if (res.ai_family == AF_INET6 && setsockopt(soap->master, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&set, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt set IPV6_V6ONLY failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#endif
#ifdef WITH_NO_IPV6_V6ONLY
  if (res.ai_family == AF_INET6 && setsockopt(soap->master, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&unset, sizeof(int)))
  { soap->errnum = soap_socket_errno(soap->master);
    soap_set_receiver_error(soap, tcp_error(soap), "setsockopt unset IPV6_V6ONLY failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#endif
  soap->errmode = 0;
  if (bind(soap->master, res.ai_addr, (int)res.ai_addrlen))
  { soap->errnum = soap_socket_errno(soap->master);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not bind to host\n"));
    soap_closesock(soap);
    soap_set_receiver_error(soap, tcp_error(soap), "bind failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#else
  soap->peerlen = sizeof(soap->peer.in);
  memset((void*)&soap->peer.in, 0, sizeof(soap->peer.in));
  soap->peer.in.sin_family = AF_INET;
  soap->errmode = 2;
  if (host)
  { if (soap->fresolve(soap, host, &soap->peer.in.sin_addr))
    { soap_set_receiver_error(soap, tcp_error(soap), "get host by name failed in soap_bind()", SOAP_TCP_ERROR);
      return SOAP_INVALID_SOCKET;
    }
  }
  else
    soap->peer.in.sin_addr.s_addr = htonl(INADDR_ANY);
  soap->peer.in.sin_port = htons((short)port);
  soap->errmode = 0;
  if (bind(soap->master, &soap->peer.addr, (int)soap->peerlen))
  { soap->errnum = soap_socket_errno(soap->master);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not bind to host\n"));
    soap_closesock(soap);
    soap_set_receiver_error(soap, tcp_error(soap), "bind failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#endif
  if (!(soap->omode & SOAP_IO_UDP) && listen(soap->master, backlog))
  { soap->errnum = soap_socket_errno(soap->master);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not bind to host\n"));
    soap_closesock(soap);
    soap_set_receiver_error(soap, tcp_error(soap), "listen failed in soap_bind()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
  return soap->master;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_poll(struct soap *soap)
{
#ifndef WITH_LEAN
  int r;
  if (soap_valid_socket(soap->socket))
  { r = tcp_select(soap, soap->socket, SOAP_TCP_SELECT_ALL, 0);
    if (r > 0 && (r & SOAP_TCP_SELECT_ERR))
      r = -1;
  }
  else if (soap_valid_socket(soap->master))
    r = tcp_select(soap, soap->master, SOAP_TCP_SELECT_SND, 0);
  else
    return SOAP_OK; /* OK when no socket! */
  if (r > 0)
  {
#ifdef WITH_OPENSSL
    if (soap->imode & SOAP_ENC_SSL)
    {
      if (soap_valid_socket(soap->socket)
       && (r & SOAP_TCP_SELECT_SND)
       && (!(r & SOAP_TCP_SELECT_RCV)
        || SSL_peek(soap->ssl, soap->tmpbuf, 1) > 0))
        return SOAP_OK;
    }
    else
#endif
    { int t;
      if (soap_valid_socket(soap->socket)
       && (r & SOAP_TCP_SELECT_SND)
       && (!(r & SOAP_TCP_SELECT_RCV)
        || recv(soap->socket, (char*)&t, 1, MSG_PEEK) > 0))
        return SOAP_OK;
    }
  }
  else if (r < 0)
  { if ((soap_valid_socket(soap->master) || soap_valid_socket(soap->socket)) && soap_socket_errno(soap->master) != SOAP_EINTR)
    { soap_set_receiver_error(soap, tcp_error(soap), "select failed in soap_poll()", SOAP_TCP_ERROR);
      return soap->error = SOAP_TCP_ERROR;
    }
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Polling: other end down on socket=%d select=%d\n", soap->socket, r));
  return SOAP_EOF;
#else
  (void)soap;
  return SOAP_OK;
#endif
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
SOAP_FMAC1
SOAP_SOCKET
SOAP_FMAC2
soap_accept(struct soap *soap)
{ int n = (int)sizeof(soap->peer);
  int err;
#ifndef WITH_LEAN
#ifndef WIN32
  int len = sizeof(soap->buf);
#else
  int len = sizeof(soap->buf) + 1; /* speeds up windows xfer */
#endif
  int set = 1;
#endif
  soap->error = SOAP_OK;
  memset((void*)&soap->peer, 0, sizeof(soap->peer));
  soap->socket = SOAP_INVALID_SOCKET;
  soap->errmode = 0;
  soap->keep_alive = 0;
  if (!soap_valid_socket(soap->master))
  { soap->errnum = 0;
    soap_set_receiver_error(soap, tcp_error(soap), "no master socket in soap_accept()", SOAP_TCP_ERROR);
    return SOAP_INVALID_SOCKET;
  }
#ifndef WITH_LEAN
  if ((soap->omode & SOAP_IO_UDP))
    return soap->socket = soap->master;
#endif
  for (;;)
  { if (soap->accept_timeout || soap->send_timeout || soap->recv_timeout)
    { for (;;)
      { int r;
        r = tcp_select(soap, soap->master, SOAP_TCP_SELECT_ALL, soap->accept_timeout ? soap->accept_timeout : 60);
        if (r > 0)
          break;
        if (!r && soap->accept_timeout)
        { soap_set_receiver_error(soap, "Timeout", "accept failed in soap_accept()", SOAP_TCP_ERROR);
          return SOAP_INVALID_SOCKET;
        }
        if (r < 0)
        { r = soap->errnum;
          if (r != SOAP_EINTR)
          { soap_closesock(soap);
            soap_set_receiver_error(soap, tcp_error(soap), "accept failed in soap_accept()", SOAP_TCP_ERROR);
            return SOAP_INVALID_SOCKET;
          }
        }
      }
    }
    if (soap->accept_timeout)
      SOAP_SOCKNONBLOCK(soap->master)
    else
      SOAP_SOCKBLOCK(soap->master)
    n = (int)sizeof(soap->peer);
    soap->socket = soap->faccept(soap, soap->master, &soap->peer.addr, &n);
    soap->peerlen = (size_t)n;
    if (soap_valid_socket(soap->socket))
    {
#ifdef WITH_IPV6
      char *s = soap->host;
      char port[16];
      int i;
      getnameinfo(&soap->peer.addr, n, soap->host, sizeof(soap->host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
      soap->ip = 0;
      for (i = 0; i < 4 && *s; i++)
      { soap->ip = (soap->ip << 8) + (unsigned int)soap_strtoul(s, &s, 10);
	if (*s)
	  s++;
      }
      soap->port = soap_strtol(port, NULL, 10);
#else
      soap->ip = ntohl(soap->peer.in.sin_addr.s_addr);
      (SOAP_SNPRINTF(soap->host, sizeof(soap->host), 80), "%u.%u.%u.%u", (int)(soap->ip>>24)&0xFF, (int)(soap->ip>>16)&0xFF, (int)(soap->ip>>8)&0xFF, (int)soap->ip&0xFF);
      soap->port = (int)ntohs(soap->peer.in.sin_port); /* does not return port number on some systems */
#endif
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Accept socket=%d at port=%d from IP='%s'\n", soap->socket, soap->port, soap->host));
#ifndef WITH_LEAN
      if ((soap->accept_flags & SO_LINGER))
      { struct linger linger;
        memset((void*)&linger, 0, sizeof(linger));
        linger.l_onoff = 1;
        linger.l_linger = soap->linger_time;
        if (setsockopt(soap->socket, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(struct linger)))
        { soap->errnum = soap_socket_errno(soap->socket);
          soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_LINGER failed in soap_accept()", SOAP_TCP_ERROR);
          soap_closesock(soap);
          return SOAP_INVALID_SOCKET;
        }
      }
      if ((soap->accept_flags & ~SO_LINGER) && setsockopt(soap->socket, SOL_SOCKET, soap->accept_flags & ~SO_LINGER, (char*)&set, sizeof(int)))
      { soap->errnum = soap_socket_errno(soap->socket);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt failed in soap_accept()", SOAP_TCP_ERROR);
        soap_closesock(soap);
        return SOAP_INVALID_SOCKET;
      }
#ifndef UNDER_CE
      if (((soap->imode | soap->omode) & SOAP_IO_KEEPALIVE) && setsockopt(soap->socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&set, sizeof(int)))
      { soap->errnum = soap_socket_errno(soap->socket);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_KEEPALIVE failed in soap_accept()", SOAP_TCP_ERROR);
        soap_closesock(soap);
        return SOAP_INVALID_SOCKET;
      }
      if (setsockopt(soap->socket, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
      { soap->errnum = soap_socket_errno(soap->socket);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_SNDBUF failed in soap_accept()", SOAP_TCP_ERROR);
        soap_closesock(soap);
        return SOAP_INVALID_SOCKET;
      }
      if (setsockopt(soap->socket, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
      { soap->errnum = soap_socket_errno(soap->socket);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt SO_RCVBUF failed in soap_accept()", SOAP_TCP_ERROR);
        soap_closesock(soap);
        return SOAP_INVALID_SOCKET;
      }
#ifdef TCP_NODELAY
      if (setsockopt(soap->socket, IPPROTO_TCP, TCP_NODELAY, (char*)&set, sizeof(int)))
      { soap->errnum = soap_socket_errno(soap->socket);
        soap_set_receiver_error(soap, tcp_error(soap), "setsockopt TCP_NODELAY failed in soap_accept()", SOAP_TCP_ERROR);
        soap_closesock(soap);
        return SOAP_INVALID_SOCKET;
      }
#endif
#endif
#endif
      soap->keep_alive = (((soap->imode | soap->omode) & SOAP_IO_KEEPALIVE) != 0);
      if (soap->send_timeout || soap->recv_timeout)
        SOAP_SOCKNONBLOCK(soap->socket)
      else
        SOAP_SOCKBLOCK(soap->socket)
      return soap->socket;
    }
    err = soap_socket_errno(soap->socket);
    if (err != 0 && err != SOAP_EINTR && err != SOAP_EAGAIN && err != SOAP_EWOULDBLOCK)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Accept failed from %s\n", soap->host));
      soap->errnum = err;
      soap_set_receiver_error(soap, tcp_error(soap), "accept failed in soap_accept()", SOAP_TCP_ERROR);
      soap_closesock(soap);
      return SOAP_INVALID_SOCKET;
    }
  }
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_closesock(struct soap *soap)
{ int status = soap->error;
#ifndef WITH_LEANER
  if (status) /* close on error: attachment state is not to be trusted */
  { soap->mime.first = NULL;
    soap->mime.last = NULL;
    soap->dime.first = NULL;
    soap->dime.last = NULL;
  }
#endif
  if (soap->fdisconnect && (soap->error = soap->fdisconnect(soap)))
    return soap->error;
  if (status == SOAP_EOF || status == SOAP_TCP_ERROR || status == SOAP_SSL_ERROR || !soap->keep_alive)
  { if (soap->fclose && (soap->error = soap->fclose(soap)))
      return soap->error;
    soap->keep_alive = 0;
  }
#ifdef WITH_ZLIB
  if (!(soap->mode & SOAP_MIME_POSTCHECK))
  { if (soap->zlib_state == SOAP_ZLIB_DEFLATE)
      deflateEnd(soap->d_stream);
    else if (soap->zlib_state == SOAP_ZLIB_INFLATE)
      inflateEnd(soap->d_stream);
    soap->zlib_state = SOAP_ZLIB_NONE;
  }
#endif
  return soap->error = status;
}
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_force_closesock(struct soap *soap)
{ soap->keep_alive = 0;
  if (soap_valid_socket(soap->socket))
    return soap_closesocket(soap->socket);
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_cleanup(struct soap *soap)
{ soap_done(soap);
#ifdef WIN32
  if (!tcp_done)
    return;
  tcp_done = 0;
  WSACleanup();
#endif
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_done(struct soap *soap)
{
#ifdef SOAP_DEBUG
  int i;
#endif
  if (soap_check_state(soap))
    return;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Done with context%s\n", soap->state == SOAP_COPY ? " copy" : ""));
  soap_free_temp(soap);
  while (soap->clist)
  { struct soap_clist *p = soap->clist->next;
    SOAP_FREE(soap, soap->clist);
    soap->clist = p;
  }
  if (soap->state == SOAP_INIT)
    soap->omode &= ~SOAP_IO_UDP; /* to force close the socket */
  soap->keep_alive = 0; /* to force close the socket */
  if (soap->master == soap->socket) /* do not close twice */
    soap->master = SOAP_INVALID_SOCKET;
  soap_closesock(soap);
#ifdef WITH_COOKIES
  soap_free_cookies(soap);
#endif
  while (soap->plugins)
  { struct soap_plugin *p = soap->plugins->next;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Removing plugin '%s'\n", soap->plugins->id));
    if (soap->plugins->fcopy || soap->state == SOAP_INIT)
      soap->plugins->fdelete(soap, soap->plugins);
    SOAP_FREE(soap, soap->plugins);
    soap->plugins = p;
  }
  soap->fplugin = fplugin;
  soap->fmalloc = NULL;
#ifndef WITH_NOHTTP
  soap->fpost = http_post;
  soap->fget = http_get;
  soap->fput = http_405;
  soap->fdel = http_405;
  soap->fopt = http_200;
  soap->fhead = http_200;
  soap->fform = NULL;
  soap->fposthdr = http_post_header;
  soap->fresponse = http_response;
  soap->fparse = http_parse;
  soap->fparsehdr = http_parse_header;
#endif
  soap->fheader = NULL;
#ifndef WITH_NOIO
#ifndef WITH_IPV6
  soap->fresolve = tcp_gethost;
#else
  soap->fresolve = NULL;
#endif
  soap->faccept = tcp_accept;
  soap->fopen = tcp_connect;
  soap->fclose = tcp_disconnect;
  soap->fclosesocket = tcp_closesocket;
  soap->fshutdownsocket = tcp_shutdownsocket;
  soap->fsend = fsend;
  soap->frecv = frecv;
  soap->fpoll = soap_poll;
#else
  soap->fopen = NULL;
  soap->fclose = NULL;
  soap->fpoll = NULL;
#endif
#ifndef WITH_LEANER
  soap->fsvalidate = NULL;
  soap->fwvalidate = NULL;
  soap->feltbegin = NULL;
  soap->feltendin = NULL;
  soap->feltbegout = NULL;
  soap->feltendout = NULL;
  soap->fprepareinitsend = NULL;
  soap->fprepareinitrecv = NULL;
  soap->fpreparesend = NULL;
  soap->fpreparerecv = NULL;
  soap->fpreparefinalsend = NULL;
  soap->fpreparefinalrecv = NULL;
  soap->ffiltersend = NULL;
  soap->ffilterrecv = NULL;
#endif
  soap->fseterror = NULL;
  soap->fignore = NULL;
  soap->fserveloop = NULL;
#ifdef WITH_OPENSSL
  if (soap->session)
  { SSL_SESSION_free(soap->session);
    soap->session = NULL;
  }
#endif
  if (soap->state == SOAP_INIT)
  { if (soap_valid_socket(soap->master))
    { soap->fclosesocket(soap, soap->master);
      soap->master = SOAP_INVALID_SOCKET;
    }
  }
#ifdef WITH_OPENSSL
  if (soap->ssl)
  { SSL_free(soap->ssl);
    soap->ssl = NULL;
  }
  if (soap->state == SOAP_INIT)
  { if (soap->ctx)
    { SSL_CTX_free(soap->ctx);
      soap->ctx = NULL;
    }
  }
  ERR_remove_state(0);
#endif
#ifdef WITH_GNUTLS
  if (soap->state == SOAP_INIT)
  { if (soap->xcred)
    { gnutls_certificate_free_credentials(soap->xcred);
      soap->xcred = NULL;
    }
    if (soap->acred)
    { gnutls_anon_free_client_credentials(soap->acred);
      soap->acred = NULL;
    }
    if (soap->cache)
    { gnutls_priority_deinit(soap->cache);
      soap->cache = NULL;
    }
    if (soap->dh_params)
    { gnutls_dh_params_deinit(soap->dh_params);
      soap->dh_params = NULL;
    }
    if (soap->rsa_params)
    { gnutls_rsa_params_deinit(soap->rsa_params);
      soap->rsa_params = NULL;
    }
  }
  if (soap->session)
  { gnutls_deinit(soap->session);
    soap->session = NULL;
  }
#endif
#ifdef WITH_SYSTEMSSL
  if (soap->ssl)
    gsk_secure_socket_close(&soap->ssl);
  if (soap->state == SOAP_INIT)
    if (soap->ctx)
      gsk_environment_close(&soap->ctx);
#endif  
#ifdef WITH_C_LOCALE
  if (soap->c_locale)
  {
# ifdef WIN32
    _free_locale(soap->c_locale);
# else
    freelocale(soap->c_locale);
# endif
    soap->c_locale = NULL;
  }
#endif
#ifdef WITH_ZLIB
  if (soap->d_stream)
  { SOAP_FREE(soap, soap->d_stream);
    soap->d_stream = NULL;
  }
  if (soap->z_buf)
  { SOAP_FREE(soap, soap->z_buf);
    soap->z_buf = NULL;
  }
#endif
#ifdef SOAP_DEBUG
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free logfiles\n"));
  for (i = 0; i < SOAP_MAXLOGS; i++)
  { if (soap->logfile[i])
    { SOAP_FREE(soap, soap->logfile[i]);
      soap->logfile[i] = NULL;
    }
    soap_close_logfile(soap, i);
  }
  soap->state = SOAP_NONE;
#endif
#ifdef SOAP_MEM_DEBUG
  soap_free_mht(soap);
#endif
}
#endif

/******************************************************************************\
 *
 *	HTTP
 *
\******************************************************************************/

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_parse(struct soap *soap)
{ char header[SOAP_HDRLEN], *s;
  unsigned short httpcmd = 0;
  int status = 0;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Waiting for HTTP request/response...\n"));
  *soap->endpoint = '\0';
#ifdef WITH_NTLM
  if (!soap->ntlm_challenge)
#endif
  { soap->userid = NULL;
    soap->passwd = NULL;
    soap->authrealm = NULL;
  }
#ifdef WITH_NTLM
  soap->ntlm_challenge = NULL;
#endif
  soap->proxy_from = NULL;
  do
  { soap->length = 0;
    soap->http_content = NULL;
    soap->action = NULL;
    soap->status = 0;
    soap->body = 1;
    if (soap_getline(soap, soap->msgbuf, sizeof(soap->msgbuf)))
    { if (soap->error == SOAP_EOF)
        return SOAP_EOF;
      return soap->error = 414;
    }
    if ((s = strchr(soap->msgbuf, ' ')))
    { soap->status = (unsigned short)soap_strtoul(s, &s, 10);
      if (!soap_blank((soap_wchar)*s))
        soap->status = 0;
    }
    else
      soap->status = 0;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP status: %s\n", soap->msgbuf));
    for (;;)
    { if (soap_getline(soap, header, SOAP_HDRLEN))
      { if (soap->error == SOAP_EOF)
        { soap->error = SOAP_OK;
          DBGLOG(TEST, SOAP_MESSAGE(fdebug, "EOF in HTTP header, continue anyway\n"));
          break;
        }
        return soap->error;
      }
      if (!*header)
        break;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP header: %s\n", header));
      s = strchr(header, ':');
      if (s)
      { char *t;
        *s = '\0';
        do s++;
        while (*s && *s <= 32);
        if (*s == '"')
          s++;
        t = s + strlen(s) - 1;
        while (t > s && *t <= 32)
          t--;
        if (t >= s && *t == '"')
          t--;
        t[1] = '\0';
        if ((soap->error = soap->fparsehdr(soap, header, s)))
        { if (soap->error < SOAP_STOP)
            return soap->error;
          status = soap->error;
          soap->error = SOAP_OK;
        }
      }
    }
  } while (soap->status == 100);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Finished HTTP header parsing, status = %d\n", soap->status));
  s = strstr(soap->msgbuf, "HTTP/");
  if (s && s[7] != '1')
  { if (soap->keep_alive == 1)
      soap->keep_alive = 0;
    if (soap->status == 0 && (soap->omode & SOAP_IO) == SOAP_IO_CHUNK) /* soap->status == 0 for HTTP request */
      soap->omode = (soap->omode & ~SOAP_IO) | SOAP_IO_STORE; /* HTTP 1.0 does not support chunked transfers */
  }
  if (soap->keep_alive < 0)
    soap->keep_alive = 1;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Keep alive connection = %d\n", soap->keep_alive));
  if (soap->status == 0)
  { size_t l = 0;
    if (s)
    { if (!strncmp(soap->msgbuf, "POST ", l = 5))
        httpcmd = 1;
      else if (!strncmp(soap->msgbuf, "PUT ", l = 4))
        httpcmd = 2;
      else if (!strncmp(soap->msgbuf, "GET ", l = 4))
        httpcmd = 3;
      else if (!strncmp(soap->msgbuf, "DELETE ", l = 7))
        httpcmd = 4;
      else if (!strncmp(soap->msgbuf, "OPTIONS ", l = 8))
        httpcmd = 5;
      else if (!strncmp(soap->msgbuf, "HEAD ", l = 5))
        httpcmd = 6;
    }
    if (s && httpcmd)
    { size_t m, n, k;
      while (soap_blank(soap->msgbuf[l]))
	l++;
      m = strlen(soap->endpoint);
      n = m + (s - soap->msgbuf) - l - 1;
      if (n >= sizeof(soap->endpoint))
        n = sizeof(soap->endpoint) - 1;
      if (m > n)
        m = n;
      k = n - m + 1;
      if (k >= sizeof(soap->path))
        k = sizeof(soap->path) - 1;
      while (k > 0 && soap_blank(soap->msgbuf[l + k - 1]))
	k--;
      soap_strncpy(soap->path, sizeof(soap->path), soap->msgbuf + l, k);
      if (*soap->path && *soap->path != '/')
	soap_strncpy(soap->endpoint, sizeof(soap->endpoint), soap->path, k);
      else
	soap_strncat(soap->endpoint, sizeof(soap->endpoint), soap->path, k);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Target endpoint='%s' path='%s'\n", soap->endpoint, soap->path));
      if (httpcmd > 1)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP %s handler\n", soap->msgbuf));
        switch (httpcmd)
        { case  2: soap->error = soap->fput(soap); break;
          case  3: soap->error = soap->fget(soap); break;
          case  4: soap->error = soap->fdel(soap); break;
          case  5: soap->error = soap->fopt(soap); break;
          case  6: soap->error = soap->fhead(soap); break;
          default: soap->error = 405; break;
        }
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP handler return = %d\n", soap->error));
        if (soap->error == SOAP_OK)
          soap->error = SOAP_STOP; /* prevents further processing */
        return soap->error;
      }
      if (status)
        return soap->error = status;
    }
    else if (status)
      return soap->error = status;
    else if (s)
      return soap->error = 405;
    return SOAP_OK;
  }
#if 0
  if (soap->length > 0 || (soap->http_content && (!soap->keep_alive || soap->recv_timeout)) || (soap->imode & SOAP_IO) == SOAP_IO_CHUNK)
#endif
  if (soap->body)
  { if ((soap->status >= 200 && soap->status <= 299) /* OK, Accepted, etc */
     || soap->status == 400                          /* Bad Request */
     || soap->status == 500)                         /* Internal Server Error */
      return soap->error = SOAP_OK;
    /* force close afterwards in soap_closesock() */
    soap->keep_alive = 0;
#ifndef WITH_LEAN
    /* read HTTP body for error details */
    s = soap_get_http_body(soap, NULL);
    if (s)
      return soap_set_receiver_error(soap, soap->msgbuf, s, soap->status);
#endif
  }
  else if (soap->status >= 200 && soap->status <= 299)
    return soap->error = soap->status;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP error %d: %s\n", soap->status, soap->msgbuf));
  return soap_set_receiver_error(soap, "HTTP Error", soap->msgbuf, soap->status);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_parse_header(struct soap *soap, const char *key, const char *val)
{ if (!soap_tag_cmp(key, "Host"))
  {
#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
    if (soap->imode & SOAP_ENC_SSL)
      soap_strcpy(soap->endpoint, sizeof(soap->endpoint), "https://");
    else
#endif
      soap_strcpy(soap->endpoint, sizeof(soap->endpoint), "http://");
    soap_strncat(soap->endpoint, sizeof(soap->endpoint), val, sizeof(soap->endpoint) - 9);
  }
#ifndef WITH_LEANER
  else if (!soap_tag_cmp(key, "Content-Type"))
  { const char *action;
    soap->http_content = soap_strdup(soap, val);
    if (soap_get_header_attribute(soap, val, "application/dime"))
      soap->imode |= SOAP_ENC_DIME;
    else if (soap_get_header_attribute(soap, val, "multipart/related")
          || soap_get_header_attribute(soap, val, "multipart/form-data"))
    { soap->mime.boundary = soap_strdup(soap, soap_get_header_attribute(soap, val, "boundary"));
      soap->mime.start = soap_strdup(soap, soap_get_header_attribute(soap, val, "start"));
      soap->imode |= SOAP_ENC_MIME;
    }
    action = soap_get_header_attribute(soap, val, "action");
    if (action)
    { if (*action == '"')
      { soap->action = soap_strdup(soap, action + 1);
        if (*soap->action)
          soap->action[strlen(soap->action) - 1] = '\0';
      }
      else
        soap->action = soap_strdup(soap, action);
    }
  }
#endif
  else if (!soap_tag_cmp(key, "Content-Length"))
  { soap->length = soap_strtoul(val, NULL, 10);
    if (!soap->length)
      soap->body = 0;
  }
  else if (!soap_tag_cmp(key, "Content-Encoding"))
  { if (!soap_tag_cmp(val, "deflate"))
#ifdef WITH_ZLIB
      soap->zlib_in = SOAP_ZLIB_DEFLATE;
#else
      return SOAP_ZLIB_ERROR;
#endif
    else if (!soap_tag_cmp(val, "gzip"))
#ifdef WITH_GZIP
      soap->zlib_in = SOAP_ZLIB_GZIP;
#else
      return SOAP_ZLIB_ERROR;
#endif
  }
#ifdef WITH_ZLIB
  else if (!soap_tag_cmp(key, "Accept-Encoding"))
  {
#ifdef WITH_GZIP
    if (strchr(val, '*') || soap_get_header_attribute(soap, val, "gzip"))
      soap->zlib_out = SOAP_ZLIB_GZIP;
    else
#endif
    if (strchr(val, '*') || soap_get_header_attribute(soap, val, "deflate"))
      soap->zlib_out = SOAP_ZLIB_DEFLATE;
    else
      soap->zlib_out = SOAP_ZLIB_NONE;
  }
#endif
  else if (!soap_tag_cmp(key, "Transfer-Encoding"))
  { soap->imode &= ~SOAP_IO;
    if (!soap_tag_cmp(val, "chunked"))
      soap->imode |= SOAP_IO_CHUNK;
  }
  else if (!soap_tag_cmp(key, "Connection"))
  { if (!soap_tag_cmp(val, "keep-alive"))
      soap->keep_alive = -soap->keep_alive;
    else if (!soap_tag_cmp(val, "close"))
      soap->keep_alive = 0;
  }
#if !defined(WITH_LEAN) || defined(WITH_NTLM)
  else if (!soap_tag_cmp(key, "Authorization") || !soap_tag_cmp(key, "Proxy-Authorization"))
  {
#ifdef WITH_NTLM
    if (!soap_tag_cmp(val, "NTLM*"))
      soap->ntlm_challenge = soap_strdup(soap, val + 4);
    else
#endif
    if (!soap_tag_cmp(val, "Basic *"))
    { int n;
      char *s;
      soap_base642s(soap, val + 6, soap->tmpbuf, sizeof(soap->tmpbuf) - 1, &n);
      soap->tmpbuf[n] = '\0';
      if ((s = strchr(soap->tmpbuf, ':')))
      { *s = '\0';
        soap->userid = soap_strdup(soap, soap->tmpbuf);
        soap->passwd = soap_strdup(soap, s + 1);
      }
    }
  }
  else if (!soap_tag_cmp(key, "WWW-Authenticate") || !soap_tag_cmp(key, "Proxy-Authenticate"))
  {
#ifdef WITH_NTLM
    if (!soap_tag_cmp(val, "NTLM*"))
      soap->ntlm_challenge = soap_strdup(soap, val + 4);
    else
#endif
      soap->authrealm = soap_strdup(soap, soap_get_header_attribute(soap, val + 6, "realm"));
  }
  else if (!soap_tag_cmp(key, "Expect"))
  { if (!soap_tag_cmp(val, "100-continue"))
    { if ((soap->error = soap->fposthdr(soap, "HTTP/1.1 100 Continue", NULL))
       || (soap->error = soap->fposthdr(soap, NULL, NULL)))
        return soap->error;
    }
  }
#endif
  else if (!soap_tag_cmp(key, "SOAPAction"))
  { if (*val == '"')
    { soap->action = soap_strdup(soap, val + 1);
      if (*soap->action)
        soap->action[strlen(soap->action) - 1] = '\0';
    }
    else
      soap->action = soap_strdup(soap, val);
  }
  else if (!soap_tag_cmp(key, "Location"))
  { soap_strcpy(soap->endpoint, sizeof(soap->endpoint), val);
  }
  else if (!soap_tag_cmp(key, "X-Forwarded-For"))
  { soap->proxy_from = soap_strdup(soap, val);
  }
#ifdef WITH_COOKIES
  else if (!soap_tag_cmp(key, "Cookie")
   || !soap_tag_cmp(key, "Cookie2")
   || !soap_tag_cmp(key, "Set-Cookie")
   || !soap_tag_cmp(key, "Set-Cookie2"))
  { soap_getcookies(soap, val);
  }
#endif
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#if !defined(WITH_NOHTTP) || !defined(WITH_LEANER)
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_get_header_attribute(struct soap *soap, const char *line, const char *key)
{ const char *s = line;
  if (s)
  { while (*s)
    { short flag;
      s = soap_decode_key(soap->tmpbuf, sizeof(soap->tmpbuf), s);
      flag = soap_tag_cmp(soap->tmpbuf, key);
      s = soap_decode_val(soap->tmpbuf, sizeof(soap->tmpbuf), s);
      if (!flag)
        return soap->tmpbuf;
    }
  }
  return NULL;
}
#endif
#endif

/******************************************************************************/
#if !defined(WITH_NOHTTP) || !defined(WITH_LEANER)
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_decode_key(char *buf, size_t len, const char *val)
{ return soap_decode(buf, len, val, "=,;");
}
#endif
#endif

/******************************************************************************/
#if !defined(WITH_NOHTTP) || !defined(WITH_LEANER)
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_decode_val(char *buf, size_t len, const char *val)
{ if (*val != '=')
  { *buf = '\0';
    return val;
  }
  return soap_decode(buf, len, val + 1, ",;");
}
#endif
#endif

/******************************************************************************/
#if !defined(WITH_NOHTTP) || !defined(WITH_LEANER)
#ifndef PALM_1
static const char*
soap_decode(char *buf, size_t len, const char *val, const char *sep)
{ const char *s;
  char *t = buf;
  size_t i = len;
  for (s = val; *s; s++)
    if (*s != ' ' && *s != '\t' && !strchr(sep, *s))
      break;
  if (len > 0)
  { if (*s == '"')
    { s++;
      while (*s && *s != '"' && --i)
        *t++ = *s++;
    }
    else
    { while (*s && !soap_blank((soap_wchar)*s) && !strchr(sep, *s) && --i)
      { if (*s == '%' && s[1] && s[2])
        { *t++ = ((s[1] >= 'A' ? (s[1] & 0x7) + 9 : s[1] - '0') << 4)
                + (s[2] >= 'A' ? (s[2] & 0x7) + 9 : s[2] - '0');
          s += 3;
        }
        else
          *t++ = *s++;
      }
    }
    buf[len - 1] = '\0'; /* appease */
  }
  *t = '\0';
  while (*s && !strchr(sep, *s))
    s++;
  return s;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static const char*
http_error(struct soap *soap, int status)
{ const char *msg = SOAP_STR_EOS;
  (void)soap;
  (void)status;
#ifndef WITH_LEAN
  msg = soap_code_str(h_http_error_codes, status);
  if (!msg)
    msg = SOAP_STR_EOS;
#endif
  return msg;
}
#endif
#endif

/******************************************************************************/

#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_get(struct soap *soap)
{ (void)soap;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP GET request\n"));
  return SOAP_GET_METHOD;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_405(struct soap *soap)
{ (void)soap;
  return 405;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_200(struct soap *soap)
{ return soap_send_empty_response(soap, 200);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_post(struct soap *soap, const char *endpoint, const char *host, int port, const char *path, const char *action, size_t count)
{ const char *s;
  int err;
  size_t l;
  switch (soap->status)
  { case SOAP_GET: 
      s = "GET";
      break;
    case SOAP_PUT: 
      s = "PUT";
      break;
    case SOAP_DEL: 
      s = "DELETE";
      break;
    case SOAP_CONNECT:
      s = "CONNECT";
      break;
    default:
      s = "POST";
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP %s to %s\n", s, endpoint ? endpoint : "(null)"));
#ifdef PALM
  if (!endpoint || (soap_tag_cmp(endpoint, "http:*") && soap_tag_cmp(endpoint, "https:*") && strncmp(endpoint, "_beam:", 6) && strncmp(endpoint, "_local:", 7) && strncmp(endpoint, "_btobex:", 8))
#else
  if (!endpoint || (soap_tag_cmp(endpoint, "http:*") && soap_tag_cmp(endpoint, "https:*") && soap_tag_cmp(endpoint, "httpg:*")))
#endif
    return SOAP_OK;
  /* set l to prevent overruns ('host' and 'soap->host' are substrings of 'endpoint') */
  l = strlen(endpoint) + strlen(soap->http_version) + 80;
  if (l > sizeof(soap->tmpbuf))
    return soap->error = SOAP_EOM;
  if (soap->status == SOAP_CONNECT)
    (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l), "%s %s:%d HTTP/%s", s, soap->host, soap->port, soap->http_version);
  else if (soap->proxy_host && endpoint)
    (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l), "%s %s HTTP/%s", s, endpoint, soap->http_version);
  else
    (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l), "%s /%s HTTP/%s", s, (*path == '/' ? path + 1 : path), soap->http_version);
  if ((err = soap->fposthdr(soap, soap->tmpbuf, NULL)))
    return err;
#ifdef WITH_OPENSSL
  if ((soap->ssl && port != 443) || (!soap->ssl && port != 80))
#else
  if (port != 80)
#endif
  {
#ifdef WITH_IPV6
    if (*host != '[' && strchr(host, ':'))
      (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l), "[%s]:%d", host, port); /* RFC 2732 */
    else
#endif
      (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l), "%s:%d", host, port);
  }
  else
  {
#ifdef WITH_IPV6
    if (*host != '[' && strchr(host, ':'))
      (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l), "[%s]", host); /* RFC 2732 */
    else
#endif
      soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), host);
  }
  if ((err = soap->fposthdr(soap, "Host", soap->tmpbuf)))
    return err;
  if ((err = soap->fposthdr(soap, "User-Agent", "gSOAP/2.8")))
    return err;
  if ((err = soap_puthttphdr(soap, SOAP_OK, count)))
    return err;
#ifdef WITH_ZLIB
#ifdef WITH_GZIP
  if ((err = soap->fposthdr(soap, "Accept-Encoding", "gzip, deflate")))
#else
  if ((err = soap->fposthdr(soap, "Accept-Encoding", "deflate")))
#endif
    return err;
#endif
#if !defined(WITH_LEAN) || defined(WITH_NTLM)
#ifdef WITH_NTLM
  if (soap->ntlm_challenge)
  { l = strlen(soap->ntlm_challenge);
    if (l)
    { (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), l + 5), "NTLM %s", soap->ntlm_challenge);
      if (soap->proxy_host)
      { if ((err = soap->fposthdr(soap, "Proxy-Authorization", soap->tmpbuf)))
          return err;
      }
      else if ((err = soap->fposthdr(soap, "Authorization", soap->tmpbuf)))
        return err;
    }
  }
  else
  {
#endif
  if (soap->userid && soap->passwd)
  { l = strlen(soap->userid) + strlen(soap->passwd);
    soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), "Basic ");
    (SOAP_SNPRINTF(soap->tmpbuf + 262, sizeof(soap->tmpbuf) - 262, l + 1), "%s:%s", soap->userid, soap->passwd);
    soap_s2base64(soap, (const unsigned char*)(soap->tmpbuf + 262), soap->tmpbuf + 6, (int)strlen(soap->tmpbuf + 262));
    if ((err = soap->fposthdr(soap, "Authorization", soap->tmpbuf)))
      return err;
  }
  if (soap->proxy_userid && soap->proxy_passwd)
  { l = strlen(soap->proxy_userid) + strlen(soap->proxy_passwd);
    soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), "Basic ");
    (SOAP_SNPRINTF(soap->tmpbuf + 262, sizeof(soap->tmpbuf) - 262, l + 1), "%s:%s", soap->proxy_userid, soap->proxy_passwd);
    soap_s2base64(soap, (const unsigned char*)(soap->tmpbuf + 262), soap->tmpbuf + 6, (int)strlen(soap->tmpbuf + 262));
    if ((err = soap->fposthdr(soap, "Proxy-Authorization", soap->tmpbuf)))
      return err;
  }
#ifdef WITH_NTLM
  }
#endif
#endif
#ifdef WITH_COOKIES
#ifdef WITH_OPENSSL
  if (soap_putcookies(soap, host, path, soap->ssl != NULL))
    return soap->error;
#else
  if (soap_putcookies(soap, host, path, 0))
    return soap->error;
#endif
#endif
  if (action && soap->status != SOAP_GET && soap->status != SOAP_DEL)
  { (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), strlen(action) + 2), "\"%s\"", action);
    if ((err = soap->fposthdr(soap, "SOAPAction", soap->tmpbuf)))
      return err;
  }
  return soap->fposthdr(soap, NULL, NULL);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_send_header(struct soap *soap, const char *s)
{ const char *t;
  do
  { t = strchr(s, '\n'); /* disallow \n in HTTP headers */
    if (!t)
      t = s + strlen(s);
    if (soap_send_raw(soap, s, t - s))
      return soap->error;
    s = t + 1;
  } while (*t);
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_post_header(struct soap *soap, const char *key, const char *val)
{ if (key)
  { if (http_send_header(soap, key))
      return soap->error;
    if (val && (soap_send_raw(soap, ": ", 2) || http_send_header(soap, val)))
      return soap->error;
  }
  return soap_send_raw(soap, "\r\n", 2);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
static int
http_response(struct soap *soap, int status, size_t count)
{ int err;
  char http[16];
  int code = status;
  const char *line;
#ifdef WMW_RPM_IO
  if (soap->rpmreqid)
    httpOutputEnable(soap->rpmreqid);
#endif
#ifdef WMW_RPM_IO
  if (soap->rpmreqid || soap_valid_socket(soap->master) || soap_valid_socket(soap->socket)) /* RPM behaves as if standalone */
#else
  if (soap_valid_socket(soap->master) || soap_valid_socket(soap->socket)) /* standalone application (socket) or CGI (stdin/out)? */
#endif
    (SOAP_SNPRINTF(http, sizeof(http), strlen(soap->http_version) + 5), "HTTP/%s", soap->http_version);
  else
    soap_strcpy(http, sizeof(http), "Status:");
  if (!status || status == SOAP_HTML || status == SOAP_FILE)
  { if (count || ((soap->omode & SOAP_IO) == SOAP_IO_CHUNK))
      code = 200;
    else
      code = 202;
  }
  else if (status < 200 || status >= 600)
  { const char *s = *soap_faultcode(soap);
    if (status >= SOAP_GET_METHOD && status <= SOAP_HTTP_METHOD)
      code = 405;
    else if (soap->version == 2 && (!s || !strcmp(s, "SOAP-ENV:Sender")))
      code = 400;
    else
      code = 500;
  }
  line = http_error(soap, code);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "HTTP Status = %d %s\n", code, line));
  (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), sizeof(http) + 22 + strlen(line)), "%s %d %s", http, code, line);
  if ((err = soap->fposthdr(soap, soap->tmpbuf, NULL)))
    return err;
#ifndef WITH_LEAN
  if (status == 401)
  { (SOAP_SNPRINTF_SAFE(soap->tmpbuf, sizeof(soap->tmpbuf)), "Basic realm=\"%s\"", (soap->authrealm && strlen(soap->authrealm) + 14 < sizeof(soap->tmpbuf)) ? soap->authrealm : "gSOAP Web Service");
    if ((err = soap->fposthdr(soap, "WWW-Authenticate", soap->tmpbuf)))
      return err;
  }
  else if ((status >= 301 && status <= 303) || status == 307)
  { if ((err = soap->fposthdr(soap, "Location", soap->endpoint)))
      return err;
  }
#endif
  if ((err = soap->fposthdr(soap, "Server", "gSOAP/2.8"))
   || (err = soap_puthttphdr(soap, status, count)))
    return err;
#ifdef WITH_COOKIES
  if (soap_putsetcookies(soap))
    return soap->error;
#endif
  return soap->fposthdr(soap, NULL, NULL);
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_response(struct soap *soap, int status)
{ size_t count;
  if (!(soap->omode & (SOAP_ENC_XML | SOAP_IO_STORE /* this tests for chunking too */))
   && (status == SOAP_HTML || status == SOAP_FILE))
    soap->omode = (soap->omode & ~SOAP_IO) | SOAP_IO_STORE;
  soap->status = status;
  count = soap_count_attachments(soap);
  if (soap_begin_send(soap))
    return soap->error;
#ifndef WITH_NOHTTP
  if ((soap->mode & SOAP_IO) != SOAP_IO_STORE && !(soap->mode & SOAP_ENC_XML))
  { int n = soap->mode;
    soap->mode &= ~(SOAP_IO | SOAP_ENC_ZLIB);
    if ((n & SOAP_IO) != SOAP_IO_FLUSH)
      soap->mode |= SOAP_IO_BUFFER;
    if ((soap->error = soap->fresponse(soap, status, count)))
      return soap->error;
#ifndef WITH_LEANER
    if ((n & SOAP_IO) == SOAP_IO_CHUNK)
    { if (soap_flush(soap))
        return soap->error;
    }
#endif
    soap->mode = n;
  }
#endif
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_url(struct soap *soap, const char *s, const char *t)
{ if (!t || (*t != '/' && *t != '?'))
    return s;
  (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(s) + strlen(t)), "%s%s", s, t);
  return soap->msgbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_encode_url(const char *s, char *t, size_t len)
{ int c;
  size_t n = len;
  while ((c = *s++) && --n > 0)
  { if (c > ' ' && c < 128 && !strchr("()<>@,;:\\\"/[]?={}#!$&'*+", c))
      *t++ = c;
    else if (n > 2)
    { *t++ = '%';
      *t++ = (c >> 4) + (c > 159 ? '7' : '0');
      c &= 0xF;
      *t++ = c + (c > 9 ? '7' : '0');
      n -= 2;
    }
    else
      break;
  }
  *t = '\0';
  return len - n;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_encode_url_string(struct soap *soap, const char *s)
{ if (s)
  { size_t n = 3*strlen(s)+1;
    char *t = (char*)soap_malloc(soap, n);
    if (t)
    { soap_encode_url(s, t, n);
      return t;
    }
  }
  return SOAP_STR_EOS;
}
#endif

/******************************************************************************\
 *
 *	HTTP Cookies
 *
\******************************************************************************/

#ifdef WITH_COOKIES
/******************************************************************************/
SOAP_FMAC1
struct soap_cookie*
SOAP_FMAC2
soap_cookie(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie *p;
  if (!domain)
    domain = soap->cookie_domain;
  if (!path)
    path = soap->cookie_path;
  if (!path)
    path = SOAP_STR_EOS;
  else if (*path == '/')
    path++;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Search cookie='%s' domain='%s' path='%s'\n", name, domain ? domain : "(null)", path ? path : "(null)"));
  for (p = soap->cookies; p; p = p->next)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Cookie in database: %s='%s' domain='%s' path='%s' env=%hd\n", p->name, p->value ? p->value : "(null)", p->domain ? p->domain : "(null)", p->path ? p->path : "(null)", p->env));
    if (!strcmp(p->name, name)
     && ((!p->domain && !domain) || (p->domain && !strcmp(p->domain, domain)))
     && ((!p->path && !path) || (p->path && !strncmp(p->path, path, strlen(p->path)))))
      break;
  }
  return p;
}

/******************************************************************************/
SOAP_FMAC1
struct soap_cookie*
SOAP_FMAC2
soap_set_cookie(struct soap *soap, const char *name, const char *value, const char *domain, const char *path)
{ struct soap_cookie **p, *q;
  int n;
  if (!domain)
    domain = soap->cookie_domain;
  if (!path)
    path = soap->cookie_path;
  if (!path)
    path = SOAP_STR_EOS;
  else if (*path == '/')
    path++;
  q = soap_cookie(soap, name, domain, path);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Set %scookie: %s='%s' domain='%s' path='%s'\n", q ? SOAP_STR_EOS : "new ", name, value ? value : "(null)", domain ? domain : "(null)", path ? path : "(null)"));
  if (!q)
  { if ((q = (struct soap_cookie*)SOAP_MALLOC(soap, sizeof(struct soap_cookie))))
    { size_t l = strlen(name);
      if ((q->name = (char*)SOAP_MALLOC(soap, l + 1)))
        soap_strcpy(q->name, l + 1, name);
      q->value = NULL;
      q->domain = NULL;
      q->path = NULL;
      q->expire = 0;
      q->maxage = -1;
      q->version = 1;
      q->secure = 0;
      q->modified = 0;
      for (p = &soap->cookies, n = soap->cookie_max; *p && n; p = &(*p)->next, n--)
        if (!strcmp((*p)->name, name) && (*p)->path && path && strcmp((*p)->path, path) < 0)
          break;
      if (n)
      { q->next = *p;
        *p = q;
      }
      else
      { SOAP_FREE(soap, q->name);
        SOAP_FREE(soap, q);
        q = NULL;
      }
    }
  }
  else
    q->modified = 1;
  if (q)
  { if (q->value)
    { if (!value || strcmp(value, q->value))
      { SOAP_FREE(soap, q->value);
        q->value = NULL;
      }
    }
    if (value && *value && !q->value)
    { size_t l = strlen(value);
      q->value = (char*)SOAP_MALLOC(soap, l + 1);
      if (q->value)
	soap_strcpy(q->value, l + 1, value);
    }
    if (q->domain)
    { if (!domain || strcmp(domain, q->domain))
      { SOAP_FREE(soap, q->domain);
        q->domain = NULL;
      }
    }
    if (domain && !q->domain)
    { size_t l = strlen(domain);
      q->domain = (char*)SOAP_MALLOC(soap, l + 1);
      if (q->domain)
	soap_strcpy(q->domain, l + 1, domain);
    }
    if (q->path)
    { if (!path || strncmp(path, q->path, strlen(q->path)))
      { SOAP_FREE(soap, q->path);
        q->path = NULL;
      }
    }
    if (path && !q->path)
    { size_t l = strlen(path);
      q->path = (char*)SOAP_MALLOC(soap, l + 1);
      if (q->path)
	soap_strcpy(q->path, l + 1, path);
    }
    q->session = 1;
    q->env = 0;
  }
  return q;
}

/******************************************************************************/
SOAP_FMAC1
void
SOAP_FMAC2
soap_clr_cookie(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie **p, *q;
  if (!domain)
    domain = soap->cookie_domain;
  if (!domain)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Error in clear cookie='%s': cookie domain not set\n", name ? name : "(null)"));
    return;
  }
  if (!path)
    path = soap->cookie_path;
  if (!path)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Error in clear cookie='%s': cookie path not set\n", name ? name : "(null)"));
    return;
  }
  if (*path == '/')
    path++;
  for (p = &soap->cookies, q = *p; q; q = *p)
  { if (!strcmp(q->name, name) && !strcmp(q->domain, domain) && !strncmp(q->path, path, strlen(q->path)))
    { if (q->value)
        SOAP_FREE(soap, q->value);
      if (q->domain)
        SOAP_FREE(soap, q->domain);
      if (q->path)
        SOAP_FREE(soap, q->path);
      *p = q->next;
      SOAP_FREE(soap, q);
    }
    else
      p = &q->next;
  }
}

/******************************************************************************/
SOAP_FMAC1
char *
SOAP_FMAC2
soap_cookie_value(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie *p;
  if ((p = soap_cookie(soap, name, domain, path)))
    return p->value;
  return NULL;
}

/******************************************************************************/
SOAP_FMAC1
char *
SOAP_FMAC2
soap_env_cookie_value(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie *p;
  if ((p = soap_cookie(soap, name, domain, path)) && p->env)
    return p->value;
  return NULL;
}

/******************************************************************************/
SOAP_FMAC1
time_t
SOAP_FMAC2
soap_cookie_expire(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie *p;
  if ((p = soap_cookie(soap, name, domain, path)))
    return p->expire;
  return -1;
}

/******************************************************************************/
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_cookie_expire(struct soap *soap, const char *name, long expire, const char *domain, const char *path)
{ struct soap_cookie *p;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Set cookie expiration max-age=%ld: cookie='%s' domain='%s' path='%s'\n", expire, name, domain ? domain : "(null)", path ? path : "(null)"));
  if ((p = soap_cookie(soap, name, domain, path)))
  { p->maxage = expire;
    p->modified = 1;
    return SOAP_OK;
  }
  return SOAP_ERR;
}

/******************************************************************************/
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_cookie_session(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie *p;
  if ((p = soap_cookie(soap, name, domain, path)))
  { p->session = 1;
    p->modified = 1;
    return SOAP_OK;
  }
  return SOAP_ERR;
}

/******************************************************************************/
SOAP_FMAC1
int
SOAP_FMAC2
soap_clr_cookie_session(struct soap *soap, const char *name, const char *domain, const char *path)
{ struct soap_cookie *p;
  if ((p = soap_cookie(soap, name, domain, path)))
  { p->session = 0;
    p->modified = 1;
    return SOAP_OK;
  }
  return SOAP_ERR;
}

/******************************************************************************/
SOAP_FMAC1
int
SOAP_FMAC2
soap_putsetcookies(struct soap *soap)
{ struct soap_cookie *p;
  char *s, tmp[4096];
  const char *t;
  for (p = soap->cookies; p; p = p->next)
  { if (p->modified
#ifdef WITH_OPENSSL
     || (!p->env && !soap->ssl == !p->secure)
#endif
       )
    { s = tmp;
      if (p->name)
        s += soap_encode_url(p->name, s, 4064 - (s-tmp));
      if (p->value && *p->value)
      { *s++ = '=';
        s += soap_encode_url(p->value, s, 4064 - (s-tmp));
      }
      if (p->domain && (int)strlen(p->domain) < 4064 - (s-tmp))
      { soap_strcpy(s, 4096 - (s-tmp), ";Domain=");
        soap_strcpy(s + 8, 4088 - (s-tmp), p->domain);
      }
      else if (soap->cookie_domain && (int)strlen(soap->cookie_domain) < 4064 - (s-tmp))
      { soap_strcpy(s, 4096 - (s-tmp), ";Domain=");
        soap_strcpy(s + 8, 4088 - (s-tmp), soap->cookie_domain);
      }
      s += strlen(s);
      soap_strcpy(s, 4096 - (s-tmp), ";Path=/");
      if (p->path)
        t = p->path;
      else
        t = soap->cookie_path;
      if (t)
      { if (*t == '/')
          t++;
        if ((int)strlen(t) < 4064 - (s-tmp))
        { if (strchr(t, '%'))	/* already URL encoded? */
          { soap_strcpy(s, 4096 - (s-tmp), t);
            s += strlen(s);
          }
          else
            s += soap_encode_url(t, s, 4064 - (s-tmp));
        }
      }
      if (p->version > 0 && s-tmp < 4064)
      { (SOAP_SNPRINTF(s, 4096 - (s-tmp), 29), ";Version=%u", p->version);
        s += strlen(s);
      }
      if (p->maxage >= 0 && s-tmp < 4064)
      { (SOAP_SNPRINTF(s, 4096 - (s-tmp), 29), ";Max-Age=%ld", p->maxage);
        s += strlen(s);
      }
      if (s-tmp < 4073
       && (p->secure
#ifdef WITH_OPENSSL
       || soap->ssl
#endif
         ))
        soap_strcpy(s, 4096 - (s-tmp), ";Secure");
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Set-Cookie: %s\n", tmp));
      if ((soap->error = soap->fposthdr(soap, "Set-Cookie", tmp)))
        return soap->error;
    }
  }
  return SOAP_OK;
}

/******************************************************************************/
SOAP_FMAC1
int
SOAP_FMAC2
soap_putcookies(struct soap *soap, const char *domain, const char *path, int secure)
{ struct soap_cookie **p, *q;
  unsigned int version = 0;
  time_t now = time(NULL);
  char *s, tmp[4096];
  if (!domain || !path)
    return SOAP_OK;
  s = tmp;
  p = &soap->cookies;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Sending cookies for domain='%s' path='%s'\n", domain, path));
  if (*path == '/')
    path++;
  while ((q = *p))
  { if (q->expire && now > q->expire)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Cookie %s expired\n", q->name));
      SOAP_FREE(soap, q->name);
      if (q->value)
        SOAP_FREE(soap, q->value);
      if (q->domain)
        SOAP_FREE(soap, q->domain);
      if (q->path)
        SOAP_FREE(soap, q->path);
      *p = q->next;
      SOAP_FREE(soap, q);
    }
    else
    { int flag;
      char *t = q->domain;
      size_t n = 0;
      if (!t)
        flag = 1;
      else
      { const char *r = strchr(t, ':');
        if (r)
          n = r - t;
        else
          n = strlen(t);
        flag = !strncmp(t, domain, n);
      }
      /* domain-level cookies, cannot compile when WITH_NOIO set */
#ifndef WITH_NOIO
      if (!flag)
      { struct hostent *hostent = gethostbyname((char*)domain);
        if (hostent)
        { const char *r = hostent->h_name;
          if (*t == '.')
          { size_t k = strlen(hostent->h_name);
            if (k >= n)
              r = hostent->h_name + k - n;
          }
          flag = !strncmp(t, r, n);
          DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Domain cookie %s host %s (match=%d)\n", t, r, flag));
        }
      }
#endif
      if (flag
          && (!q->path || !strncmp(q->path, path, strlen(q->path)))
          && (!q->secure || secure))
      { size_t n = 12;
        if (q->name)
          n += 3*strlen(q->name);
        if (q->value && *q->value)
          n += 3*strlen(q->value) + 1;
        if (q->path && *q->path)
          n += strlen(q->path) + 9;
        if (q->domain)
          n += strlen(q->domain) + 11;
        if (tmp - s + n > sizeof(tmp))
        { if (s == tmp)
            return SOAP_OK; /* HTTP header size overflow */
          DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Cookie: %s\n", tmp));
          if ((soap->error = soap->fposthdr(soap, "Cookie", tmp)))
            return soap->error;
          s = tmp;
        }
        else if (s != tmp)
        { *s++ = ' ';
        }
        if (q->version != version && s-tmp < 4060)
        { (SOAP_SNPRINTF(s, 4096 - (s-tmp), 29), "$Version=%u;", q->version);
          version = q->version;
          s += strlen(s);
        }
        if (q->name)
          s += soap_encode_url(q->name, s, tmp+sizeof(tmp)-s-16);
        if (q->value && *q->value)
        { *s++ = '=';
          s += soap_encode_url(q->value, s, tmp+sizeof(tmp)-s-16);
        }
        if (q->path && (s-tmp) + strlen(q->path) < 4060)
        { (SOAP_SNPRINTF_SAFE(s, 4096 - (s-tmp)), ";$Path=\"/%s\"", (*q->path == '/' ? q->path + 1 : q->path));
          s += strlen(s);
        }
        if (q->domain && (s-tmp) + strlen(q->domain) < 4060)
        { (SOAP_SNPRINTF_SAFE(s, 4096 - (s-tmp)), ";$Domain=\"%s\"", q->domain);
          s += strlen(s);
        }
      }
      p = &q->next;
    }
  }
  if (s != tmp)
    if ((soap->error = soap->fposthdr(soap, "Cookie", tmp)))
      return soap->error;
  return SOAP_OK;
}

/******************************************************************************/
SOAP_FMAC1
void
SOAP_FMAC2
soap_getcookies(struct soap *soap, const char *val)
{ struct soap_cookie *p = NULL, *q;
  const char *s;
  char *t, tmp[4096]; /* cookie size is up to 4096 bytes [RFC2109] */
  char *domain = NULL;
  char *path = NULL;
  unsigned int version = 0;
  time_t now = time(NULL);
  if (!val)
    return;
  s = val;
  while (*s)
  { s = soap_decode_key(tmp, sizeof(tmp), s);
    if (!soap_tag_cmp(tmp, "$Version"))
    { if ((s = soap_decode_val(tmp, sizeof(tmp), s)))
      { if (p)
          p->version = (int)soap_strtol(tmp, NULL, 10);
        else
          version = (int)soap_strtol(tmp, NULL, 10);
      }
    }
    else if (!soap_tag_cmp(tmp, "$Path"))
    { s = soap_decode_val(tmp, sizeof(tmp), s);
      if (*tmp)
      { size_t l = strlen(tmp) + 1;
	if ((t = (char*)SOAP_MALLOC(soap, l)))
          soap_memcpy((void*)t, l, (const void*)tmp, l);
      }
      else
        t = NULL;
      if (p)
      { if (p->path)
          SOAP_FREE(soap, p->path);
        p->path = t;
      }
      else
      { if (path)
          SOAP_FREE(soap, path);
        path = t;
      }
    }
    else if (!soap_tag_cmp(tmp, "$Domain"))
    { s = soap_decode_val(tmp, sizeof(tmp), s);
      if (*tmp)
      { size_t l = strlen(tmp) + 1;
        if ((t = (char*)SOAP_MALLOC(soap, l)))
          soap_memcpy((void*)t, l, (const void*)tmp, l);
      }
      else
        t = NULL;
      if (p)
      { if (p->domain)
          SOAP_FREE(soap, p->domain);
        p->domain = t;
      }
      else
      { if (domain)
          SOAP_FREE(soap, domain);
        domain = t;
      }
    }
    else if (p && !soap_tag_cmp(tmp, "Path"))
    { if (p->path)
        SOAP_FREE(soap, p->path);
      s = soap_decode_val(tmp, sizeof(tmp), s);
      if (*tmp)
      { size_t l = strlen(tmp) + 1;
        if ((p->path = (char*)SOAP_MALLOC(soap, l)))
          soap_memcpy((void*)p->path, l, (const void*)tmp, l);
      }
      else
        p->path = NULL;
    }
    else if (p && !soap_tag_cmp(tmp, "Domain"))
    { if (p->domain)
        SOAP_FREE(soap, p->domain);
      s = soap_decode_val(tmp, sizeof(tmp), s);
      if (*tmp)
      { size_t l = strlen(tmp) + 1;
        if ((p->domain = (char*)SOAP_MALLOC(soap, l)))
          soap_memcpy((void*)p->domain, l, (const void*)tmp, l);
      }
      else
        p->domain = NULL;
    }
    else if (p && !soap_tag_cmp(tmp, "Version"))
    { s = soap_decode_val(tmp, sizeof(tmp), s);
      p->version = (unsigned int)soap_strtoul(tmp, NULL, 10);
    }
    else if (p && !soap_tag_cmp(tmp, "Max-Age"))
    { s = soap_decode_val(tmp, sizeof(tmp), s);
      p->expire = now + soap_strtol(tmp, NULL, 10);
    }
    else if (p && !soap_tag_cmp(tmp, "Expires"))
    { struct tm T;
      char a[3];
      static const char mns[] = "anebarprayunulugepctovec";
      s = soap_decode_val(tmp, sizeof(tmp), s);
      if (strlen(tmp) > 20)
      { memset((void*)&T, 0, sizeof(T));
        a[0] = tmp[4];
        a[1] = tmp[5];
        a[2] = '\0';
        T.tm_mday = (int)soap_strtol(a, NULL, 10);
        a[0] = tmp[8];
        a[1] = tmp[9];
        T.tm_mon = (int)(strstr(mns, a) - mns) / 2;
        a[0] = tmp[11];
        a[1] = tmp[12];
        T.tm_year = 100 + (int)soap_strtol(a, NULL, 10);
        a[0] = tmp[13];
        a[1] = tmp[14];
        T.tm_hour = (int)soap_strtol(a, NULL, 10);
        a[0] = tmp[16];
        a[1] = tmp[17];
        T.tm_min = (int)soap_strtol(a, NULL, 10);
        a[0] = tmp[19];
        a[1] = tmp[20];
        T.tm_sec = (int)soap_strtol(a, NULL, 10);
        p->expire = soap_timegm(&T);
      }
    }
    else if (p && !soap_tag_cmp(tmp, "Secure"))
      p->secure = 1;
    else
    { if (p)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Got environment cookie='%s' value='%s' domain='%s' path='%s' expire=%ld secure=%d\n", p->name, p->value ? p->value : "(null)", p->domain ? p->domain : "(null)", p->path ? p->path : "(null)", p->expire, p->secure));
        if ((q = soap_set_cookie(soap, p->name, p->value, p->domain, p->path)))
        { q->version = p->version;
          q->expire = p->expire;
          q->secure = p->secure;
          q->env = 1;
        }
        if (p->name)
          SOAP_FREE(soap, p->name);
        if (p->value)
          SOAP_FREE(soap, p->value);
        if (p->domain)
          SOAP_FREE(soap, p->domain);
        if (p->path)
          SOAP_FREE(soap, p->path);
        SOAP_FREE(soap, p);
      }
      if ((p = (struct soap_cookie*)SOAP_MALLOC(soap, sizeof(struct soap_cookie))))
      { size_t l = strlen(tmp) + 1;
	if ((p->name = (char*)SOAP_MALLOC(soap, l)))
	  soap_memcpy((void*)p->name, l, (const void*)tmp, l);
        s = soap_decode_val(tmp, sizeof(tmp), s);
        if (*tmp)
        { l = strlen(tmp) + 1;
	  if ((p->value = (char*)SOAP_MALLOC(soap, l)))
	    soap_memcpy((void*)p->value, l, (const void*)tmp, l);
        }
        else
          p->value = NULL;
        if (domain)
          p->domain = domain;
        else if (*soap->host)
        { l = strlen(soap->host) + 1;
          if ((p->domain = (char*)SOAP_MALLOC(soap, l)))
            soap_memcpy((void*)p->domain, l, (const void*)soap->host, l);
        }
        else
          p->domain = NULL;
        if (path)
          p->path = path;
        else if (*soap->path)
        { l = strlen(soap->path) + 1;
          if ((p->path = (char*)SOAP_MALLOC(soap, l)))
            soap_memcpy((void*)p->path, l, (const void*)soap->path, l);
        }
        else
        { if ((p->path = (char*)SOAP_MALLOC(soap, 2)))
            soap_memcpy((void*)p->path, 2, (const void*)"/", 2);
        }
        p->expire = 0;
        p->secure = 0;
        p->version = version;
      }
    }
  }
  if (p)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Got environment cookie='%s' value='%s' domain='%s' path='%s' expire=%ld secure=%d\n", p->name, p->value ? p->value : "(null)", p->domain ? p->domain : "(null)", p->path ? p->path : "(null)", p->expire, p->secure));
    if ((q = soap_set_cookie(soap, p->name, p->value, p->domain, p->path)))
    { q->version = p->version;
      q->expire = p->expire;
      q->secure = p->secure;
      q->env = 1;
    }
    if (p->name)
      SOAP_FREE(soap, p->name);
    if (p->value)
      SOAP_FREE(soap, p->value);
    if (p->domain)
      SOAP_FREE(soap, p->domain);
    if (p->path)
      SOAP_FREE(soap, p->path);
    SOAP_FREE(soap, p);
  }
  if (domain)
    SOAP_FREE(soap, domain);
  if (path)
    SOAP_FREE(soap, path);
}

/******************************************************************************/
SOAP_FMAC1
int
SOAP_FMAC2
soap_getenv_cookies(struct soap *soap)
{ struct soap_cookie *p;
  const char *s;
  char key[4096], val[4096]; /* cookie size is up to 4096 bytes [RFC2109] */
  if (!(s = getenv("HTTP_COOKIE")))
    return SOAP_ERR;
  do
  { s = soap_decode_key(key, sizeof(key), s);
    s = soap_decode_val(val, sizeof(val), s);
    p = soap_set_cookie(soap, key, val, NULL, NULL);
    if (p)
      p->env = 1;
  } while (*s);
  return SOAP_OK;
}

/******************************************************************************/
SOAP_FMAC1
struct soap_cookie*
SOAP_FMAC2
soap_copy_cookies(struct soap *copy, const struct soap *soap)
{ struct soap_cookie *p, **q, *r;
  q = &r;
  for (p = soap->cookies; p; p = p->next)
  { if (!(*q = (struct soap_cookie*)SOAP_MALLOC(copy, sizeof(struct soap_cookie))))
      return r;
    **q = *p;
    if (p->name)
    { size_t l = strlen(p->name) + 1;
      if (((*q)->name = (char*)SOAP_MALLOC(copy, l)))
        soap_memcpy((*q)->name, l, p->name, l);
    }
    if (p->value)
    { size_t l = strlen(p->value) + 1;
      if (((*q)->value = (char*)SOAP_MALLOC(copy, l)))
        soap_memcpy((*q)->value, l, p->value, l);
    }
    if (p->domain)
    { size_t l = strlen(p->domain) + 1;
      if (((*q)->domain = (char*)SOAP_MALLOC(copy, l)))
        soap_memcpy((*q)->domain, l, p->domain, l);
    }
    if (p->path)
    { size_t l = strlen(p->path) + 1;
      if (((*q)->path = (char*)SOAP_MALLOC(copy, l)))
        soap_memcpy((*q)->path, l, p->path, l);
    }
    q = &(*q)->next;
  }
  *q = NULL;
  return r;
}

/******************************************************************************/
SOAP_FMAC1
void
SOAP_FMAC2
soap_free_cookies(struct soap *soap)
{ struct soap_cookie *p;
  for (p = soap->cookies; p; p = soap->cookies)
  { soap->cookies = p->next;
    SOAP_FREE(soap, p->name);
    if (p->value)
      SOAP_FREE(soap, p->value);
    if (p->domain)
      SOAP_FREE(soap, p->domain);
    if (p->path)
      SOAP_FREE(soap, p->path);
    SOAP_FREE(soap, p);
  }
}

/******************************************************************************/
#endif /* WITH_COOKIES */

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
size_t
SOAP_FMAC2
soap_hash(const char *s)
{ size_t h = 0;
  while (*s)
    h = 65599*h + *s++;
  return h % SOAP_IDHASH;
}
#endif

/******************************************************************************/
#ifndef PALM_1
static void
soap_init_pht(struct soap *soap)
{ int i;
  soap->pblk = NULL;
  soap->pidx = 0;
  for (i = 0; i < (int)SOAP_PTRHASH; i++)
    soap->pht[i] = NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
struct soap*
SOAP_FMAC2
soap_versioning(soap_new)(soap_mode imode, soap_mode omode)
{ struct soap *soap;
#ifdef __cplusplus
  soap = SOAP_NEW(struct soap);
#else
  soap = (struct soap*)malloc(sizeof(struct soap));
#endif
  if (soap)
    soap_versioning(soap_init)(soap, imode, omode);
  return soap;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_free(struct soap *soap)
{ soap_done(soap);
#ifdef __cplusplus
  SOAP_DELETE(soap);
#else
  free(soap);
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_del(struct soap *soap)
{ free(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_1
static void
soap_free_pht(struct soap *soap)
{ struct soap_pblk *pb, *next;
  int i;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free pointer hashtable\n"));
  for (pb = soap->pblk; pb; pb = next)
  { next = pb->next;
    SOAP_FREE(soap, pb);
  }
  soap->pblk = NULL;
  soap->pidx = 0;
  for (i = 0; i < (int)SOAP_PTRHASH; i++)
    soap->pht[i] = NULL;
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_embed(struct soap *soap, const void *p, const void *a, int n, int t)
{ int id;
  struct soap_plist *pp;
  if (soap->version == 2)
    soap->encoding = 1;
  if (!p || (!soap->encodingStyle && !(soap->omode & SOAP_XML_GRAPH)) || (soap->omode & SOAP_XML_TREE))
    return 0;
  if (a)
    id = soap_array_pointer_lookup(soap, p, a, n, t, &pp);
  else
    id = soap_pointer_lookup(soap, p, t, &pp);
  if (id)
  { if (soap_is_embedded(soap, pp)
     || soap_is_single(soap, pp))
      return 0;
    soap_set_embedded(soap, pp);
  }
  return id;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_pointer_lookup(struct soap *soap, const void *p, int type, struct soap_plist **ppp)
{ struct soap_plist *pp;
  *ppp = NULL;
  if (p)
  { for (pp = soap->pht[soap_hash_ptr(p)]; pp; pp = pp->next)
    { if (pp->ptr == p && pp->type == type)
      { *ppp = pp;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Lookup location=%p type=%d id=%d\n", p, type, pp->id));
        return pp->id;
      }
    }
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Lookup location=%p type=%d: not found\n", p, type));
  return 0;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_pointer_enter(struct soap *soap, const void *p, const void *a, int n, int type, struct soap_plist **ppp)
{ size_t h;
  struct soap_plist *pp;
  (void)n;
  if (!soap->pblk || soap->pidx >= SOAP_PTRBLK)
  { struct soap_pblk *pb = (struct soap_pblk*)SOAP_MALLOC(soap, sizeof(struct soap_pblk));
    if (!pb)
    { soap->error = SOAP_EOM;
      return 0;
    }
    pb->next = soap->pblk;
    soap->pblk = pb;
    soap->pidx = 0;
  }
  *ppp = pp = &soap->pblk->plist[soap->pidx++];
  if (a)
    h = soap_hash_ptr(a);
  else
    h = soap_hash_ptr(p);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Pointer enter location=%p array=%p size=%lu type=%d id=%d\n", p, a, (unsigned long)n, type, soap->idnum+1));
  pp->next = soap->pht[h];
  pp->type = type;
  pp->mark1 = 0;
  pp->mark2 = 0;
  pp->ptr = p;
  pp->dup = NULL;
  pp->array = a;
  pp->size = n;
  soap->pht[h] = pp;
  pp->id = ++soap->idnum;
  return pp->id;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_array_pointer_lookup(struct soap *soap, const void *p, const void *a, int n, int type, struct soap_plist **ppp)
{ struct soap_plist *pp;
  *ppp = NULL;
  if (!p || !a)
    return 0;
  for (pp = soap->pht[soap_hash_ptr(a)]; pp; pp = pp->next)
  { if (pp->type == type && pp->array == a && pp->size == n)
    { *ppp = pp;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Array lookup location=%p type=%d id=%d\n", a, type, pp->id));
      return pp->id;
    }
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Array lookup location=%p type=%d: not found\n", a, type));
  return 0;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_begin_count(struct soap *soap)
{ soap_free_ns(soap);
#ifndef WITH_LEANER
  if ((soap->mode & SOAP_ENC_DIME) || (soap->omode & SOAP_ENC_DIME))
    soap->mode = soap->omode | SOAP_IO_LENGTH | SOAP_ENC_DIME;
  else
#endif
  { soap->mode = soap->omode;
    if ((soap->mode & SOAP_IO_UDP))
    { soap->mode |= SOAP_ENC_XML;
      soap->mode &= ~SOAP_IO_CHUNK;
    }
    if ((soap->mode & SOAP_IO) == SOAP_IO_STORE
     || (((soap->mode & SOAP_IO) == SOAP_IO_CHUNK || (soap->mode & SOAP_ENC_XML))
#ifndef WITH_LEANER
      && !soap->fpreparesend
#endif
      ))
      soap->mode &= ~SOAP_IO_LENGTH;
    else
      soap->mode |= SOAP_IO_LENGTH;
  }
#ifdef WITH_ZLIB
  if ((soap->mode & SOAP_ENC_ZLIB) && (soap->mode & SOAP_IO) == SOAP_IO_FLUSH)
  { if (!(soap->mode & SOAP_ENC_DIME))
      soap->mode &= ~SOAP_IO_LENGTH;
    if (soap->mode & SOAP_ENC_XML)
      soap->mode |= SOAP_IO_BUFFER;
    else
      soap->mode |= SOAP_IO_STORE;
  }
#endif
#ifndef WITH_LEANER
  if ((soap->mode & SOAP_ENC_MTOM) && (soap->mode & SOAP_ENC_DIME))
    soap->mode |= SOAP_ENC_MIME;
  else if (!(soap->mode & SOAP_ENC_MIME))
    soap->mode &= ~SOAP_ENC_MTOM;
  if (soap->mode & SOAP_ENC_MIME)
    soap_select_mime_boundary(soap);
  soap->dime.list = soap->dime.last;	/* keep track of last DIME attachment */
#endif
  soap->count = 0;
  soap->ns = 0;
  soap->null = 0;
  soap->position = 0;
  soap->mustUnderstand = 0;
  soap->encoding = 0;
  soap->part = SOAP_BEGIN;
  soap->event = 0;
  soap->evlev = 0;
  soap->idnum = 0;
  soap->body = 1;
  soap->level = 0;
  soap_clr_attr(soap);
  soap_set_local_namespaces(soap);
#ifndef WITH_LEANER
  soap->dime.count = 0; /* count # of attachments */
  soap->dime.size = 0; /* accumulate total size of attachments */
  if (soap->fprepareinitsend && (soap->mode & SOAP_IO) != SOAP_IO_STORE && (soap->error = soap->fprepareinitsend(soap)))
    return soap->error;
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Begin count phase (socket=%d mode=0x%x count=%lu)\n", soap->socket, (unsigned int)soap->mode, (unsigned long)soap->count));
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_end_count(struct soap *soap)
{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End of count phase\n"));
#ifndef WITH_LEANER
  if ((soap->mode & SOAP_IO_LENGTH))
  { if (soap->fpreparefinalsend && (soap->error = soap->fpreparefinalsend(soap)))
      return soap->error;
  }
#else
  (void)soap;
#endif
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_begin_send(struct soap *soap)
{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Initializing for output to socket=%d/fd=%d\n", soap->socket, soap->sendfd));
  soap_free_ns(soap);
  soap->error = SOAP_OK;
  soap->mode = soap->omode | (soap->mode & (SOAP_IO_LENGTH | SOAP_ENC_DIME));
#ifndef WITH_LEAN
  if ((soap->mode & SOAP_IO_UDP))
  { soap->mode |= SOAP_ENC_XML;
    soap->mode &= ~SOAP_IO_CHUNK;
    if (soap->count > sizeof(soap->buf))
      return soap->error = SOAP_UDP_ERROR;
  }
#endif
#ifdef WITH_ZLIB
  if ((soap->mode & SOAP_ENC_ZLIB) && (soap->mode & SOAP_IO) == SOAP_IO_FLUSH)
  { if (soap->mode & SOAP_ENC_XML)
      soap->mode |= SOAP_IO_BUFFER;
    else
      soap->mode |= SOAP_IO_STORE;
  }
#endif
  if ((soap->mode & SOAP_IO) == SOAP_IO_FLUSH && soap_valid_socket(soap->socket))
  { if (soap->count || (soap->mode & SOAP_IO_LENGTH) || (soap->mode & SOAP_ENC_XML))
      soap->mode |= SOAP_IO_BUFFER;
    else
      soap->mode |= SOAP_IO_STORE;
  }
  soap->mode &= ~SOAP_IO_LENGTH;
  if ((soap->mode & SOAP_IO) == SOAP_IO_STORE)
    if (soap_new_block(soap) == NULL)
      return soap->error;
  if (!(soap->mode & SOAP_IO_KEEPALIVE))
    soap->keep_alive = 0;
#ifndef WITH_LEANER
  if ((soap->mode & SOAP_ENC_MTOM) && (soap->mode & SOAP_ENC_DIME))
  { soap->mode |= SOAP_ENC_MIME;
    soap->mode &= ~SOAP_ENC_DIME;
  }
  else if (!(soap->mode & SOAP_ENC_MIME))
    soap->mode &= ~SOAP_ENC_MTOM;
  if (soap->mode & SOAP_ENC_MIME)
    soap_select_mime_boundary(soap);
#ifdef WIN32
#ifndef UNDER_CE
#ifndef WITH_FASTCGI
  if (!soap_valid_socket(soap->socket) && !soap->os) /* Set win32 stdout or soap->sendfd to BINARY, e.g. to support DIME */
#ifdef __BORLANDC__
    setmode(soap->sendfd, _O_BINARY);
#else
    _setmode(soap->sendfd, _O_BINARY);
#endif
#endif
#endif
#endif
#endif
  if (soap->mode & SOAP_IO)
  { soap->bufidx = 0;
    soap->buflen = 0;
  }
  soap->chunksize = 0;
  soap->ns = 0;
  soap->null = 0;
  soap->position = 0;
  soap->mustUnderstand = 0;
  soap->encoding = 0;
  soap->idnum = 0;
  soap->body = 1;
  soap->level = 0;
  soap_clr_attr(soap);
  soap_set_local_namespaces(soap);
#ifdef WITH_ZLIB
  soap->z_ratio_out = 1.0;
  if ((soap->mode & SOAP_ENC_ZLIB) && soap->zlib_state != SOAP_ZLIB_DEFLATE)
  { if (!soap->d_stream)
    { soap->d_stream = (z_stream*)SOAP_MALLOC(soap, sizeof(z_stream));
      soap->d_stream->zalloc = Z_NULL;
      soap->d_stream->zfree = Z_NULL;
      soap->d_stream->opaque = Z_NULL;
      soap->d_stream->next_in = Z_NULL;
    }
    if (!soap->z_buf)
      soap->z_buf = (char*)SOAP_MALLOC(soap, sizeof(soap->buf));
    soap->d_stream->next_out = (Byte*)soap->z_buf;
    soap->d_stream->avail_out = sizeof(soap->buf);
#ifdef WITH_GZIP
    if (soap->zlib_out != SOAP_ZLIB_DEFLATE)
    { if (soap_memcpy((void*)soap->z_buf, sizeof(soap->buf), (const void*)"\37\213\10\0\0\0\0\0\0\377", 10))
        return soap->error = SOAP_EOM;
      soap->d_stream->next_out = (Byte*)soap->z_buf + 10;
      soap->d_stream->avail_out = sizeof(soap->buf) - 10;
      soap->z_crc = crc32(0L, NULL, 0);
      soap->zlib_out = SOAP_ZLIB_GZIP;
      if (soap->z_dict)
        *((Byte*)soap->z_buf + 2) = 0xff;
      if (deflateInit2(soap->d_stream, soap->z_level, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return soap->error = SOAP_ZLIB_ERROR;
    }
    else
#endif
    if (deflateInit(soap->d_stream, soap->z_level) != Z_OK)
      return soap->error = SOAP_ZLIB_ERROR;
    if (soap->z_dict)
    { if (deflateSetDictionary(soap->d_stream, (const Bytef*)soap->z_dict, soap->z_dict_len) != Z_OK)
        return soap->error = SOAP_ZLIB_ERROR;
    }
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Deflate initialized\n"));
    soap->zlib_state = SOAP_ZLIB_DEFLATE;
  }
#endif
#ifdef WITH_OPENSSL
  if (soap->ssl)
    ERR_clear_error();
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Begin send phase (socket=%d mode=0x%x count=%lu)\n", soap->socket, soap->mode, (unsigned long)soap->count));
  soap->part = SOAP_BEGIN;
#ifndef WITH_LEANER
  if (soap->fprepareinitsend && (soap->mode & SOAP_IO) == SOAP_IO_STORE && (soap->error = soap->fprepareinitsend(soap)))
    return soap->error;
#endif
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_embedded(struct soap *soap, const void *p, int t)
{ struct soap_plist *pp;
  if (soap_pointer_lookup(soap, p, t, &pp))
  { pp->mark1 = 1;
    pp->mark2 = 1;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Embedded %p type=%d mark set to 1\n", p, t));
  }
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_reference(struct soap *soap, const void *p, int t)
{ struct soap_plist *pp;
  if (!p || (!soap->encodingStyle && !(soap->omode & (SOAP_ENC_DIME|SOAP_ENC_MIME|SOAP_ENC_MTOM|SOAP_XML_GRAPH))) || (soap->omode & SOAP_XML_TREE))
    return 1;
  if (soap_pointer_lookup(soap, p, t, &pp))
  { if (pp->mark1 == 0)
    { pp->mark1 = 2;
      pp->mark2 = 2;
    }
  }
  else if (!soap_pointer_enter(soap, p, NULL, 0, t, &pp))
    return 1;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Reference %p type=%d (%d %d)\n", p, t, (int)pp->mark1, (int)pp->mark2));
  return pp->mark1;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_array_reference(struct soap *soap, const void *p, const void *a, int n, int t)
{ struct soap_plist *pp;
  if (!p || !a || (!soap->encodingStyle && !(soap->omode & (SOAP_ENC_DIME|SOAP_ENC_MIME|SOAP_ENC_MTOM|SOAP_XML_GRAPH))) || (soap->omode & SOAP_XML_TREE))
    return 1;
  if (soap_array_pointer_lookup(soap, p, a, n, t, &pp))
  { if (pp->mark1 == 0)
    { pp->mark1 = 2;
      pp->mark2 = 2;
    }
  }
  else if (!soap_pointer_enter(soap, p, a, n, t, &pp))
    return 1;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Array reference %p ptr=%p n=%lu type=%d (%d %d)\n", p, a, (unsigned long)n, t, (int)pp->mark1, (int)pp->mark2));
  return pp->mark1;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_embedded_id(struct soap *soap, int id, const void *p, int t)
{ struct soap_plist *pp = NULL;
  if (id >= 0 || (!soap->encodingStyle && !(soap->omode & SOAP_XML_GRAPH)) || (soap->omode & SOAP_XML_TREE))
    return id;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Embedded_id %p type=%d id=%d\n", p, t, id));
  if (id < -1)
    return soap_embed(soap, p, NULL, 0, t);
  if (id < 0)
  { id = soap_pointer_lookup(soap, p, t, &pp);
    if (soap->version == 1 && soap->part != SOAP_IN_HEADER)
    { if (id)
      { if (soap->mode & SOAP_IO_LENGTH)
          pp->mark1 = 2;
        else
          pp->mark2 = 2;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Embedded_id multiref id=%d %p type=%d = (%d %d)\n", id, p, t, (int)pp->mark1, (int)pp->mark2));
      }
      return -1;
    }
    else if (id)
    { if (soap->mode & SOAP_IO_LENGTH)
        pp->mark1 = 1;
      else
	pp->mark2 = 1;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Embedded_id embedded ref id=%d %p type=%d = (%d %d)\n", id, p, t, (int)pp->mark1, (int)pp->mark2));
    }
  }
  return id;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_is_embedded(struct soap *soap, struct soap_plist *pp)
{ if (!pp)
    return 0;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Is embedded? %d %d\n", (int)pp->mark1, (int)pp->mark2));
  if (soap->version == 1 && soap->encodingStyle && !(soap->mode & SOAP_XML_GRAPH) && soap->part != SOAP_IN_HEADER)
  { if (soap->mode & SOAP_IO_LENGTH)
      return pp->mark1 != 0;
    return pp->mark2 != 0;
  }
  if (soap->mode & SOAP_IO_LENGTH)
    return pp->mark1 == 1;
  return pp->mark2 == 1;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_is_single(struct soap *soap, struct soap_plist *pp)
{ if (soap->part == SOAP_IN_HEADER)
    return 1;
  if (!pp)
    return 0;
  if (soap->mode & SOAP_IO_LENGTH)
    return pp->mark1 == 0;
  return pp->mark2 == 0;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_embedded(struct soap *soap, struct soap_plist *pp)
{ if (!pp)
    return;
  if (soap->mode & SOAP_IO_LENGTH)
    pp->mark1 = 1;
  else
    pp->mark2 = 1;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_attachment(struct soap *soap, const char *tag, int id, const void *p, const void *a, int n, const char *aid, const char *atype, const char *aoptions, const char *type, int t)
{ struct soap_plist *pp;
  int i;
  if (!p || !a || (!aid && !atype))
    return soap_element_id(soap, tag, id, p, a, n, type, t, NULL);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Attachment tag='%s' id='%s' (%d) type='%s'\n", tag, aid ? aid : SOAP_STR_EOS, id, atype ? atype : SOAP_STR_EOS));
  i = soap_array_pointer_lookup(soap, p, a, n, t, &pp);
  if (!i)
  { i = soap_pointer_enter(soap, p, a, n, t, &pp);
    if (!i)
    { soap->error = SOAP_EOM;
      return -1;
    }
  }
  if (id <= 0)
    id = i;
  if (!aid)
  { (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), strlen(soap->dime_id_format) + 20), soap->dime_id_format, id);
    aid = soap_strdup(soap, soap->tmpbuf);
  }
  /* Add MTOM xop:Include element when necessary */
  /* TODO: this code to be obsoleted with new import/xop.h conventions */
  if ((soap->mode & SOAP_ENC_MTOM) && strcmp(tag, "xop:Include"))
  { if (soap_element_begin_out(soap, tag, 0, type)
     || soap_element_href(soap, "xop:Include", 0, "xmlns:xop=\"http://www.w3.org/2004/08/xop/include\" href", aid)
     || soap_element_end_out(soap, tag))
      return soap->error;
  }
  else if (soap_element_href(soap, tag, 0, "href", aid))
    return soap->error;
  if (soap->mode & SOAP_IO_LENGTH)
  { if (pp->mark1 != 3)
    { struct soap_multipart *content;
      if (soap->mode & SOAP_ENC_MTOM)
        content = soap_new_multipart(soap, &soap->mime.first, &soap->mime.last, (char*)a, n);
      else
        content = soap_new_multipart(soap, &soap->dime.first, &soap->dime.last, (char*)a, n);
      if (!content)
      { soap->error = SOAP_EOM;
        return -1;
      }
      if (!strncmp(aid, "cid:", 4)) /* RFC 2111 */
      { if (soap->mode & SOAP_ENC_MTOM)
        { size_t l = strlen(aid) - 1;
	  char *s = (char*)soap_malloc(soap, l);
          if (s)
          { s[0] = '<';
            soap_strncpy(s + 1, l - 1, aid + 4, l - 3);
            s[l - 2] = '>';
	    s[l - 1] = '\0';
            content->id = s;
          }
        }
        else
          content->id = aid + 4;
      }
      else
        content->id = aid;
      content->type = atype;
      content->options = aoptions;
      content->encoding = SOAP_MIME_BINARY;
      pp->mark1 = 3;
    }
  }
  else
    pp->mark2 = 3;
  return -1;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
static void
soap_init_iht(struct soap *soap)
{ int i;
  for (i = 0; i < SOAP_IDHASH; i++)
    soap->iht[i] = NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
static void
soap_free_iht(struct soap *soap)
{ int i;
  struct soap_ilist *ip = NULL, *p = NULL;
  struct soap_flist *fp = NULL, *fq = NULL;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free ID hashtable\n"));
  for (i = 0; i < SOAP_IDHASH; i++)
  { for (ip = soap->iht[i]; ip; ip = p)
    { for (fp = ip->flist; fp; fp = fq)
      { fq = fp->next;
        SOAP_FREE(soap, fp);
      }
      p = ip->next;
      SOAP_FREE(soap, ip);
    }
    soap->iht[i] = NULL;
  }
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
struct soap_ilist *
SOAP_FMAC2
soap_lookup(struct soap *soap, const char *id)
{ struct soap_ilist *ip = NULL;
  for (ip = soap->iht[soap_hash(id)]; ip; ip = ip->next)
    if (!strcmp(ip->id, id))
      return ip;
  return NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
struct soap_ilist *
SOAP_FMAC2
soap_enter(struct soap *soap, const char *id, int t, size_t n)
{ size_t h;
  struct soap_ilist *ip;
  size_t l = strlen(id);
  ip = (struct soap_ilist*)SOAP_MALLOC(soap, sizeof(struct soap_ilist) + l);
  if (ip)
  { ip->type = t;
    ip->size = n;
    ip->ptr = NULL;
    ip->spine = NULL;
    ip->link = NULL;
    ip->copy = NULL;
    ip->flist = NULL;
    ip->smart = NULL;
    ip->shaky = 0;
    soap_strcpy((char*)ip->id, l + 1, id);
    h = soap_hash(id); /* h = (HASH(id) % SOAP_IDHASH) so soap->iht[h] is safe */
    ip->next = soap->iht[h];
    soap->iht[h] = ip;
  }
  return ip;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void*
SOAP_FMAC2
soap_malloc(struct soap *soap, size_t n)
{ char *p;
  if (!soap)
    return SOAP_MALLOC(soap, n);
  if (soap->fmalloc)
    p = (char*)soap->fmalloc(soap, n);
  else
  { n += sizeof(short);
    n += (-(long)n) & (sizeof(void*)-1); /* align at 4-, 8- or 16-byte boundary */
    if (!(p = (char*)SOAP_MALLOC(soap, n + sizeof(void*) + sizeof(size_t))))
    { soap->error = SOAP_EOM;
      return NULL;
    }
    /* set the canary to detect corruption */
    *(unsigned short*)(p + n - sizeof(unsigned short)) = (unsigned short)SOAP_CANARY;
    /* keep chain of alloced cells for destruction */
    *(void**)(p + n) = soap->alist;
    *(size_t*)(p + n + sizeof(void*)) = n;
    soap->alist = p + n;
  }
  return p;
}
#endif

/******************************************************************************/
#ifdef SOAP_MEM_DEBUG
static void
soap_init_mht(struct soap *soap)
{ int i;
  for (i = 0; i < (int)SOAP_PTRHASH; i++)
    soap->mht[i] = NULL;
}
#endif

/******************************************************************************/
#ifdef SOAP_MEM_DEBUG
static void
soap_free_mht(struct soap *soap)
{ int i;
  struct soap_mlist *mp, *mq;
  for (i = 0; i < (int)SOAP_PTRHASH; i++)
  { for (mp = soap->mht[i]; mp; mp = mq)
    { mq = mp->next;
      if (mp->live)
        fprintf(stderr, "%s(%d): malloc() = %p not freed (memory leak or forgot to call soap_end()?)\n", mp->file, mp->line, mp->ptr);
      free(mp);
    }
    soap->mht[i] = NULL;
  }
}
#endif

/******************************************************************************/
#ifdef SOAP_MEM_DEBUG
SOAP_FMAC1
void*
SOAP_FMAC2
soap_track_malloc(struct soap *soap, const char *file, int line, size_t size)
{ void *p = malloc(size);
  if (soap)
  { size_t h = soap_hash_ptr(p);
    struct soap_mlist *mp = (struct soap_mlist*)malloc(sizeof(struct soap_mlist));
    if (soap->fdebug[SOAP_INDEX_TEST])
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "%s(%d): malloc(%lu) = %p\n", file, line, (unsigned long)size, p));
    }
    mp->next = soap->mht[h];
    mp->ptr = p;
    mp->file = file;
    mp->line = line;
    mp->live = 1;
    soap->mht[h] = mp;
  }
  return p;
}
#endif

/******************************************************************************/
#ifdef SOAP_MEM_DEBUG
SOAP_FMAC1
void
SOAP_FMAC2
soap_track_free(struct soap *soap, const char *file, int line, void *p)
{ if (!soap)
    free(p);
  else
  { size_t h = soap_hash_ptr(p);
    struct soap_mlist *mp;
    for (mp = soap->mht[h]; mp; mp = mp->next)
      if (mp->ptr == p)
	break;
    if (mp)
    { if (mp->live)
      { free(p);
	if (soap->fdebug[SOAP_INDEX_TEST])
	{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "%s(%d): free(%p)\n", file, line, p));
	}
	mp->live = 0;
      }
      else
	fprintf(stderr, "%s(%d): free(%p) double free of pointer malloced at %s(%d)\n", file, line, p, mp->file, mp->line);
    }
    else
      fprintf(stderr, "%s(%d): free(%p) pointer not malloced\n", file, line, p);
  }
}
#endif

/******************************************************************************/
#ifdef SOAP_MEM_DEBUG
static void
soap_track_unlink(struct soap *soap, const void *p)
{ size_t h = soap_hash_ptr(p);
  struct soap_mlist *mp;
  for (mp = soap->mht[h]; mp; mp = mp->next)
    if (mp->ptr == p)
      break;
  if (mp)
    mp->live = 0;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_dealloc(struct soap *soap, void *p)
{ if (soap_check_state(soap))
    return;
  if (p)
  { char **q;
    for (q = (char**)(void*)&soap->alist; *q; q = *(char***)q)
    {
      if (*(unsigned short*)(char*)(*q - sizeof(unsigned short)) != (unsigned short)SOAP_CANARY)
      {
#ifdef SOAP_MEM_DEBUG
        fprintf(stderr, "Data corruption in dynamic allocation (see logs)\n");
#endif
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Data corruption:\n"));
        DBGHEX(TEST, *q - 200, 200);
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\n"));
        soap->error = SOAP_MOE;
        return;
      }
      if (p == (void*)(*q - *(size_t*)(*q + sizeof(void*))))
      { *q = **(char***)q;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Freed data at %p\n", p));
        SOAP_FREE(soap, p);
        return;
      }
    }
    soap_delete(soap, p);
  }
  else
  { char *q;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free all soap_malloc() data\n"));
    while (soap->alist)
    { q = (char*)soap->alist;
      if (*(unsigned short*)(char*)(q - sizeof(unsigned short)) != (unsigned short)SOAP_CANARY)
      {
#ifdef SOAP_MEM_DEBUG
        fprintf(stderr, "Data corruption in dynamic allocation (see logs)\n");
#endif
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Data corruption:\n"));
        DBGHEX(TEST, q - 200, 200);
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\n"));
        soap->error = SOAP_MOE;
        return;
      }
      soap->alist = *(void**)q;
      q -= *(size_t*)(q + sizeof(void*));
      SOAP_FREE(soap, q);
    }
    /* assume these were deallocated: */
    soap->http_content = NULL;
    soap->action = NULL;
    soap->fault = NULL;
    soap->header = NULL;
    soap->userid = NULL;
    soap->passwd = NULL;
    soap->authrealm = NULL;
#ifdef WITH_NTLM
    soap->ntlm_challenge = NULL;
#endif
#ifndef WITH_LEANER
    soap_clr_mime(soap);
#endif
  }
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_delete(struct soap *soap, void *p)
{ struct soap_clist **cp;
  if (soap_check_state(soap))
    return;
  cp = &soap->clist;
  if (p)
  { while (*cp)
    { if (p == (*cp)->ptr)
      { struct soap_clist *q = *cp;
        *cp = q->next;
        if (q->fdelete(q))
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not dealloc data %p: deletion callback failed for object type %d\n", q->ptr, q->type));
#ifdef SOAP_MEM_DEBUG
          fprintf(stderr, "new(object type = %d) = %p not freed: deletion callback failed\n", q->type, q->ptr);
#endif
        }
        SOAP_FREE(soap, q);
        return;
      }
      cp = &(*cp)->next;
    }
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not dealloc data %p: address not in list\n", p));
  }
  else
  { while (*cp)
    { struct soap_clist *q = *cp;
      *cp = q->next;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Delete %p type=%d (cp=%p)\n", q->ptr, q->type, q));
      if (q->fdelete(q))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not dealloc data %p: deletion callback failed for object type %d\n", q->ptr, q->type));
#ifdef SOAP_MEM_DEBUG
        fprintf(stderr, "new(object type = %d) = %p not freed: deletion callback failed\n", q->type, q->ptr);
#endif
      }
      SOAP_FREE(soap, q);
    }
  }
  soap->fault = NULL; /* assume this was deallocated */
  soap->header = NULL; /* assume this was deallocated */
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_delegate_deletion(struct soap *soap, struct soap *soap_to)
{ struct soap_clist *cp;
  char **q;
#ifdef SOAP_MEM_DEBUG
  void *p;
  struct soap_mlist **mp, *mq;
  size_t h;
#endif
  for (q = (char**)(void*)&soap->alist; *q; q = *(char***)q)
  {
    if (*(unsigned short*)(char*)(*q - sizeof(unsigned short)) != (unsigned short)SOAP_CANARY)
    {
#ifdef SOAP_MEM_DEBUG
      fprintf(stderr, "Data corruption in dynamic allocation (see logs)\n");
#endif
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Data corruption:\n"));
      DBGHEX(TEST, *q - 200, 200);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\n"));
      soap->error = SOAP_MOE;
      return;
    }
#ifdef SOAP_MEM_DEBUG
    p = (void*)(*q - *(size_t*)(*q + sizeof(void*)));
    h = soap_hash_ptr(p);
    for (mp = &soap->mht[h]; *mp; mp = &(*mp)->next)
    { if ((*mp)->ptr == p)
      { mq = *mp;
        *mp = mq->next;
        mq->next = soap_to->mht[h];
        soap_to->mht[h] = mq;
        break;
      }
    }
#endif
  }
  *q = (char*)soap_to->alist;
  soap_to->alist = soap->alist;
  soap->alist = NULL;
#ifdef SOAP_MEM_DEBUG
  cp = soap->clist;
  while (cp)
  { h = soap_hash_ptr(cp);
    for (mp = &soap->mht[h]; *mp; mp = &(*mp)->next)
    { if ((*mp)->ptr == cp)
      { mq = *mp;
        *mp = mq->next;
        mq->next = soap_to->mht[h];
        soap_to->mht[h] = mq;
        break;
      }
    }
    cp = cp->next;
  }
#endif
  cp = soap_to->clist;
  if (cp)
  { while (cp->next)
      cp = cp->next;
    cp->next = soap->clist;
  }
  else
    soap_to->clist = soap->clist;
  soap->clist = NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
struct soap_clist *
SOAP_FMAC2
soap_link(struct soap *soap, void *p, int t, int n, int (*fdelete)(struct soap_clist*))
{ struct soap_clist *cp = NULL;
  if (soap && p)
  { if (!(cp = (struct soap_clist*)SOAP_MALLOC(soap, sizeof(struct soap_clist))))
      soap->error = SOAP_EOM;
    else
    { cp->next = soap->clist;
      cp->type = soap->alloced = t;
      cp->size = n;
      cp->ptr = p;
      cp->fdelete = fdelete;
      soap->clist = cp;
    }
  }
  return cp;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_unlink(struct soap *soap, const void *p)
{ char **q;
  struct soap_clist **cp;
  if (soap && p)
  { for (q = (char**)(void*)&soap->alist; *q; q = *(char***)q)
    { if (p == (void*)(*q - *(size_t*)(*q + sizeof(void*))))
      { *q = **(char***)q;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Unlinked data %p\n", p));
#ifdef SOAP_MEM_DEBUG
        soap_track_unlink(soap, p);
#endif
        return SOAP_OK;		/* found and removed from dealloc chain */
      }
    }
    for (cp = &soap->clist; *cp; cp = &(*cp)->next)
    { if (p == (*cp)->ptr)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Unlinked class instance %p\n", p));
        q = (char**)*cp;
        *cp = (*cp)->next;
        SOAP_FREE(soap, q);
        return SOAP_OK;		/* found and removed from dealloc chain */
      }
    }
  }
  return SOAP_ERR;
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_lookup_type(struct soap *soap, const char *id)
{ struct soap_ilist *ip;
  if (id && *id)
  { ip = soap_lookup(soap, id);
    if (ip)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Lookup id='%s' type=%d\n", id, ip->type));
      return ip->type;
    }
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "lookup type id='%s' NOT FOUND! Need to get it from xsi:type\n", id));
  return 0;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_1
SOAP_FMAC1
short
SOAP_FMAC2
soap_begin_shaky(struct soap *soap)
{ short f = soap->shaky;
  soap->shaky = 1;
  return f;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_end_shaky(struct soap *soap, short f)
{ soap->shaky = f;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
static int
soap_is_shaky(struct soap *soap, void *p)
{ (void)p;
  if (!soap->blist && !soap->shaky)
    return 0;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Shaky %p\n", p));
  return 1;
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
void*
SOAP_FMAC2
soap_id_lookup(struct soap *soap, const char *id, void **p, int t, size_t n, unsigned int k, int (*fbase)(int, int))
{ struct soap_ilist *ip;
  if (!p || !id || !*id)
    return p;
  ip = soap_lookup(soap, id); /* lookup pointer to hash table entry for string id */
  if (!ip)
  { if (!(ip = soap_enter(soap, id, t, n))) /* new hash table entry for string id */
      return NULL;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Forwarding first href='%s' type=%d location=%p (%u bytes) level=%u\n", id, t, p, (unsigned int)n, k));
    *p = NULL;
    if (k)
    { int i;
      if (k > SOAP_MAXPTRS)
	return NULL;
      if (!(ip->spine = (void**)soap_malloc(soap, SOAP_MAXPTRS * sizeof(void*))))
	return NULL;
      ip->spine[0] = NULL;
      for (i = 1; i < SOAP_MAXPTRS; i++)
        ip->spine[i] = &ip->spine[i - 1];
      *p = &ip->spine[k - 1];
    }
    else
    { ip->link = p;
      ip->shaky = soap_is_shaky(soap, (void*)p);
    }
  }
  else if ((ip->type != t || ip->size != n) && (!fbase || !fbase(ip->type, t)) && (!fbase || !fbase(t, ip->type) || soap_type_punned(soap, ip)))
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Lookup type incompatibility: ref='%s' id-type=%d ref-type=%d\n", id, ip->type, t));
    soap_id_nullify(soap, id);
    return NULL;
  }
  else if (k == 0 && ip->ptr && !ip->shaky) /* when block lists are in use, ip->ptr will change */
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolved href='%s' type=%d location=%p (%u bytes) level=%u\n", id, t, ip->ptr, (unsigned int)n, k));
    *p = ip->ptr;
  }
  else
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Forwarded href='%s' type=%d location=%p (%u bytes) level=%u\n", id, t, p, (unsigned int)n, k));
    if (fbase && fbase(t, ip->type) && !soap_type_punned(soap, ip))
    { ip->type = t;
      ip->size = n;
    }
    *p = NULL;
    if (k)
    { if (!ip->spine)
      { int i;
        if (k > SOAP_MAXPTRS)
	  return NULL;
        if (!(ip->spine = (void**)soap_malloc(soap, SOAP_MAXPTRS * sizeof(void*))))
	  return NULL;
        ip->spine[0] = NULL;
        for (i = 1; i < SOAP_MAXPTRS; i++)
          ip->spine[i] = &ip->spine[i - 1];
      }
      *p = &ip->spine[k - 1];
      if (ip->ptr && !ip->shaky)
        ip->spine[0] = ip->ptr;
    }
    else
    { void *q = ip->link;
      ip->link = p;
      ip->shaky = soap_is_shaky(soap, (void*)p);
      *p = q;
    }
  }
  return p;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
void*
SOAP_FMAC2
soap_id_forward(struct soap *soap, const char *href, void *p, size_t i, int t, int tt, size_t n, unsigned int k, void (*finsert)(struct soap*, int, int, void*, size_t, const void*, void**), int (*fbase)(int, int))
{ struct soap_ilist *ip;
  if (!p || !href || !*href)
    return p;
  ip = soap_lookup(soap, href); /* lookup pointer to hash table entry for string id */
  if (!ip)
  { if (!(ip = soap_enter(soap, href, t, n))) /* new hash table entry for string id */
      return NULL;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New entry href='%s' type=%d size=%lu level=%d location=%p\n", href, t, (unsigned long)n, k, p));
  }
  else if ((ip->type != t || ip->size != n) && k == 0)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Forward type incompatibility id='%s' expect type=%d size=%lu level=%u got type=%d size=%lu\n", href, ip->type, (unsigned long)ip->size, k, t, (unsigned long)n));
    soap_id_nullify(soap, href);
    return NULL;
  }
  if (finsert || n < sizeof(void*))
  { struct soap_flist *fp = (struct soap_flist*)SOAP_MALLOC(soap, sizeof(struct soap_flist));
    if (!fp)
    { soap->error = SOAP_EOM;
      return NULL;
    }
    if (fbase && fbase(t, ip->type) && !soap_type_punned(soap, ip))
    { ip->type = t;
      ip->size = n;
    }
    if ((ip->type != t || ip->size != n) && (!fbase || !fbase(ip->type, t)))
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Forward type incompatibility id='%s' expect type=%d size=%lu level=%u got type=%d size=%lu\n", href, ip->type, (unsigned long)ip->size, k, t, (unsigned long)n));
      soap_id_nullify(soap, href);
      return NULL;
    }
    fp->next = ip->flist;
    fp->type = tt;
    fp->ptr = p;
    fp->level = k;
    fp->index = i;
    fp->finsert = finsert;
    ip->flist = fp;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Forwarding type=%d (target type=%d) size=%lu location=%p level=%u index=%lu href='%s'\n", t, tt, (unsigned long)n, p, k, (unsigned long)i, href));
  }
  else
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Forwarding copying address %p for type=%d href='%s'\n", p, t, href));
    *(void**)p = ip->copy;
    ip->copy = p;
  }
  ip->shaky = soap_is_shaky(soap, p);
  return p;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void*
SOAP_FMAC2
soap_id_enter(struct soap *soap, const char *id, void *p, int t, size_t n, const char *type, const char *arrayType, void *(*finstantiate)(struct soap*, int, const char*, const char*, size_t*), int (*fbase)(int, int))
{
#ifndef WITH_NOIDREF
  struct soap_ilist *ip;
#endif
  (void)id; (void)fbase;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Enter id='%s' type=%d location=%p size=%lu\n", id, t, p, (unsigned long)n));
  soap->alloced = 0;
  if (!p)
  {
    if (finstantiate)
      p = finstantiate(soap, t, type, arrayType, &n); /* alloced set in soap_link() */
    else
    {
      p = soap_malloc(soap, n);
      soap->alloced = t;
    }
  }
#ifndef WITH_NOIDREF
  if (!id || !*id)
#endif
    return p;
#ifndef WITH_NOIDREF
  ip = soap_lookup(soap, id); /* lookup pointer to hash table entry for string id */
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Lookup entry id='%s' for location=%p\n", id, p));
  if (!ip)
  { if (!(ip = soap_enter(soap, id, t, n))) /* new hash table entry for string id */
      return NULL;
    ip->ptr = p;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New entry id='%s' type=%d size=%lu location=%p\n", id, t, (unsigned long)n, p));
    if (!soap->alloced)
      ip->shaky = soap_is_shaky(soap, p);
  }
  else if (ip->ptr)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Multiply defined id='%s'\n", id));
    soap_strcpy(soap->id, sizeof(soap->id), id);
    soap->error = SOAP_DUPLICATE_ID;
    return NULL;
  }
  else if ((ip->type != t || ip->size != n) && (!fbase || !fbase(t, ip->type) || soap_type_punned(soap, ip)))
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Enter type incompatibility id='%s' expect type=%d size=%lu got type=%d size=%lu\n", id, ip->type, (unsigned long)ip->size, t, (unsigned long)n));
    soap_id_nullify(soap, id);
    return NULL;
  }
  else
  { ip->type = t;
    ip->size = n;
    ip->ptr = p;
    if (!soap->alloced)
      ip->shaky = soap_is_shaky(soap, p);
    if (soap->alloced || !ip->shaky)
    { void **q; /* ptr will not change later, so resolve links now */
      if (ip->spine)
        ip->spine[0] = p;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Traversing link chain to resolve id='%s' type=%d\n", ip->id, ip->type));
      q = (void**)ip->link;
      while (q)
      { void *r = *q;
	*q = p;
	DBGLOG(TEST, SOAP_MESSAGE(fdebug, "... link %p -> %p\n", q, p));
	q = (void**)r;
      }
      ip->link = NULL;
    }
  }
  return ip->ptr;
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void**
SOAP_FMAC2
soap_id_smart(struct soap *soap, const char *id, int t, size_t n)
{ (void)soap; (void)id; (void)t; (void)n;
#ifndef WITH_NOIDREF
  if (id && *id)
  { struct soap_ilist *ip = soap_lookup(soap, id); /* lookup pointer to hash table entry for string id */
    if (!ip)
    { if (!(ip = soap_enter(soap, id, t, n))) /* new hash table entry for string id */
	return NULL;
    }
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New smart shared pointer entry id='%s' type=%d size=%lu smart=%p\n", id, t, (unsigned long)n, ip->smart));
    return &ip->smart;
  }
#endif
  return NULL;
}
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
static int
soap_type_punned(struct soap *soap, const struct soap_ilist *ip)
{ const struct soap_flist *fp;
  (void)soap;
  if (ip->ptr || ip->copy)
    return 1;
  for (fp = ip->flist; fp; fp = fp->next)
    if (fp->level == 0)
      return 1;
  return 0;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIDREF
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_id_nullify(struct soap *soap, const char *id)
{ int i;
  for (i = 0; i < SOAP_IDHASH; i++)
  { struct soap_ilist *ip;
    for (ip = soap->iht[i]; ip; ip = ip->next)
    { void *p, *q;
      for (p = ip->link; p; p = q)
      { q = *(void**)p;
	*(void**)p = NULL;
      }
      ip->link = NULL;
    }
  }
  soap_strcpy(soap->id, sizeof(soap->id), id);
  return soap->error = SOAP_HREF;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_end_send(struct soap *soap)
{
#ifndef WITH_LEANER
  int err;
  if (soap->dime.list)
  { /* SOAP body referenced attachments must appear first */
    soap->dime.last->next = soap->dime.first;
    soap->dime.first = soap->dime.list->next;
    soap->dime.list->next = NULL;
    soap->dime.last = soap->dime.list;
  }
  if (!(err = soap_putdime(soap)))
    err = soap_putmime(soap);
  soap->mime.list = NULL;
  soap->mime.first = NULL;
  soap->mime.last = NULL;
  soap->dime.list = NULL;
  soap->dime.first = NULL;
  soap->dime.last = NULL;
  if (err)
    return err;
#endif
  return soap_end_send_flush(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_end_send_flush(struct soap *soap)
{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End send mode=0x%x\n", soap->mode));
  if (soap->mode & SOAP_IO) /* need to flush the remaining data in buffer */
  { if (soap_flush(soap))
#ifdef WITH_ZLIB
    { if (soap->mode & SOAP_ENC_ZLIB && soap->zlib_state == SOAP_ZLIB_DEFLATE)
      { soap->zlib_state = SOAP_ZLIB_NONE;
        deflateEnd(soap->d_stream);
      }
      return soap->error;
    }
#else
      return soap->error;
#endif
#ifdef WITH_ZLIB
    if ((soap->mode & SOAP_ENC_ZLIB) && soap->d_stream)
    { int r;
      soap->d_stream->avail_in = 0;
      do
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Deflating remainder\n"));
        r = deflate(soap->d_stream, Z_FINISH);
        if (soap->d_stream->avail_out != sizeof(soap->buf))
        { if (soap_flush_raw(soap, soap->z_buf, sizeof(soap->buf) - soap->d_stream->avail_out))
          { soap->zlib_state = SOAP_ZLIB_NONE;
            deflateEnd(soap->d_stream);
            return soap->error;
          }
          soap->d_stream->next_out = (Byte*)soap->z_buf;
          soap->d_stream->avail_out = sizeof(soap->buf);
        }
      } while (r == Z_OK);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Deflated total %lu->%lu bytes\n", soap->d_stream->total_in, soap->d_stream->total_out));
      soap->z_ratio_out = (float)soap->d_stream->total_out / (float)soap->d_stream->total_in;
      soap->mode &= ~SOAP_ENC_ZLIB;
      soap->zlib_state = SOAP_ZLIB_NONE;
      if (deflateEnd(soap->d_stream) != Z_OK || r != Z_STREAM_END)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Unable to end deflate: %s\n", soap->d_stream->msg ? soap->d_stream->msg : SOAP_STR_EOS));
        return soap->error = SOAP_ZLIB_ERROR;
      }
#ifdef WITH_GZIP
      if (soap->zlib_out != SOAP_ZLIB_DEFLATE)
      { soap->z_buf[0] = soap->z_crc & 0xFF;
        soap->z_buf[1] = (soap->z_crc >> 8) & 0xFF;
        soap->z_buf[2] = (soap->z_crc >> 16) & 0xFF;
        soap->z_buf[3] = (soap->z_crc >> 24) & 0xFF;
        soap->z_buf[4] = soap->d_stream->total_in & 0xFF;
        soap->z_buf[5] = (soap->d_stream->total_in >> 8) & 0xFF;
        soap->z_buf[6] = (soap->d_stream->total_in >> 16) & 0xFF;
        soap->z_buf[7] = (soap->d_stream->total_in >> 24) & 0xFF;
        if (soap_flush_raw(soap, soap->z_buf, 8))
          return soap->error;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "gzip crc32=%lu\n", (unsigned long)soap->z_crc));
      }
#endif
    }
#endif
    if ((soap->mode & SOAP_IO) == SOAP_IO_STORE)
    { char *p;
#ifndef WITH_NOHTTP
      if (!(soap->mode & SOAP_ENC_XML))
      { soap->mode--;
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Sending buffered message of length %u\n", (unsigned int)soap->blist->size));
        if (soap->status >= SOAP_POST)
          soap->error = soap->fpost(soap, soap->endpoint, soap->host, soap->port, soap->path, soap->action, soap->blist->size);
        else if (soap->status != SOAP_STOP)
          soap->error = soap->fresponse(soap, soap->status, soap->blist->size);
        if (soap->error || soap_flush(soap))
          return soap->error;
        soap->mode++;
      }
#endif
      for (p = soap_first_block(soap, NULL); p; p = soap_next_block(soap, NULL))
      { DBGMSG(SENT, p, soap_block_size(soap, NULL));
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Send %u bytes to socket=%d/fd=%d\n", (unsigned int)soap_block_size(soap, NULL), soap->socket, soap->sendfd));
        if ((soap->error = soap->fsend(soap, p, soap_block_size(soap, NULL))))
        { soap_end_block(soap, NULL);
          return soap->error;
        }
      }
      soap_end_block(soap, NULL);
#ifndef WITH_LEANER
      if (soap->fpreparefinalsend && (soap->error = soap->fpreparefinalsend(soap)))
        return soap->error;
#endif
    }
#ifndef WITH_LEANER
    else if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK)
    { DBGMSG(SENT, "\r\n0\r\n\r\n", 7);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Send 7 bytes to socket=%d/fd=%d\n", soap->socket, soap->sendfd));
      if ((soap->error = soap->fsend(soap, "\r\n0\r\n\r\n", 7)))
        return soap->error;
    }
#endif
  }
#ifdef WITH_TCPFIN
#if defined(WITH_OPENSSL) || defined(WITH_SYSTEMSSL)
  if (!soap->ssl)
#endif
    if (soap_valid_socket(soap->socket) && !soap->keep_alive && !(soap->omode & SOAP_IO_UDP))
      soap->fshutdownsocket(soap, soap->socket, SOAP_SHUT_WR); /* Send TCP FIN */
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End of send phase\n"));
  soap->omode &= ~SOAP_SEC_WSUID;
  soap->count = 0;
  soap->part = SOAP_END;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_end_recv(struct soap *soap)
{ soap->part = SOAP_END;
#ifndef WITH_LEAN
  soap->wsuid = NULL;		/* reset before next send */
  soap->c14nexclude = NULL;	/* reset before next send */
  soap->c14ninclude = NULL;	/* reset before next send */
#endif
#ifndef WITH_LEANER
  soap->ffilterrecv = NULL;
  if ((soap->mode & SOAP_ENC_DIME) && soap_getdime(soap))
  { soap->dime.first = NULL;
    soap->dime.last = NULL;
    return soap->error;
  }
  soap->dime.list = soap->dime.first;
  soap->dime.first = NULL;
  soap->dime.last = NULL;
  /* Check if MIME attachments and mime-post-check flag is set, if so call soap_resolve() and return */
  if (soap->mode & SOAP_ENC_MIME)
  { if (soap->mode & SOAP_MIME_POSTCHECK)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Post checking MIME attachments\n"));
      if (!soap->keep_alive)
        soap->keep_alive = -1;
#ifndef WITH_NOIDREF
      soap_resolve(soap);
#endif
      return SOAP_OK;
    }
    if (soap_getmime(soap))
      return soap->error;
  }
  soap->mime.list = soap->mime.first;
  soap->mime.first = NULL;
  soap->mime.last = NULL;
  soap->mime.boundary = NULL;
  if (soap->xlist)
  { struct soap_multipart *content;
    for (content = soap->mime.list; content; content = content->next)
      soap_resolve_attachment(soap, content);
  }
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End of receive message ok\n"));
#ifdef WITH_ZLIB
  if ((soap->mode & SOAP_ENC_ZLIB) && soap->d_stream)
  { /* Make sure end of compressed content is reached */
    while (soap->d_stream->next_out != Z_NULL)
      if ((int)soap_get1(soap) == EOF)
        break;
    soap->mode &= ~SOAP_ENC_ZLIB;
    soap_memcpy((void*)soap->buf, sizeof(soap->buf), (const void*)soap->z_buf, sizeof(soap->buf));
    soap->bufidx = (char*)soap->d_stream->next_in - soap->z_buf;
    soap->buflen = soap->z_buflen;
    soap->zlib_state = SOAP_ZLIB_NONE;
    if (inflateEnd(soap->d_stream) != Z_OK)
      return soap->error = SOAP_ZLIB_ERROR;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflate end ok\n"));
#ifdef WITH_GZIP
    if (soap->zlib_in == SOAP_ZLIB_GZIP)
    { soap_wchar c;
      short i;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflate gzip crc check\n"));
      for (i = 0; i < 8; i++)
      { if ((int)(c = soap_get1(soap)) == EOF)
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Gzip error: unable to read crc value\n"));
          return soap->error = SOAP_ZLIB_ERROR;
        }
        soap->z_buf[i] = (char)c;
      }
      if (soap->z_crc != ((uLong)(unsigned char)soap->z_buf[0] | ((uLong)(unsigned char)soap->z_buf[1] << 8) | ((uLong)(unsigned char)soap->z_buf[2] << 16) | ((uLong)(unsigned char)soap->z_buf[3] << 24)))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Gzip inflate error: crc check failed, message corrupted? (crc32=%lu)\n", (unsigned long)soap->z_crc));
        return soap->error = SOAP_ZLIB_ERROR;
      }
      if (soap->d_stream->total_out != ((uLong)(unsigned char)soap->z_buf[4] | ((uLong)(unsigned char)soap->z_buf[5] << 8) | ((uLong)(unsigned char)soap->z_buf[6] << 16) | ((uLong)(unsigned char)soap->z_buf[7] << 24)))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Gzip inflate error: incorrect message length\n"));
        return soap->error = SOAP_ZLIB_ERROR;
      }
    }
    soap->zlib_in = SOAP_ZLIB_NONE;
#endif
  }
#endif
  if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK)
    while (soap->ahead != EOF && !soap_recv_raw(soap))
      continue;
#ifndef WITH_NOIDREF
  if (soap_resolve(soap))
    return soap->error;
#endif
#ifndef WITH_LEANER
  if (soap->xlist)
  { if (soap->mode & SOAP_ENC_MTOM)
      return soap->error = SOAP_MIME_HREF;
    return soap->error = SOAP_DIME_HREF;
  }
#endif
  soap_free_ns(soap);
#ifndef WITH_LEANER
  if (soap->fpreparefinalrecv)
    return soap->error = soap->fpreparefinalrecv(soap);
#endif
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_free_temp(struct soap *soap)
{ struct soap_attribute *tp, *tq;
  struct Namespace *ns;
  soap_free_ns(soap);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free any remaining temp blocks\n"));
  while (soap->blist)
    soap_end_block(soap, NULL);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free attribute storage\n"));
  for (tp = soap->attributes; tp; tp = tq)
  { tq = tp->next;
    if (tp->value)
      SOAP_FREE(soap, tp->value);
    SOAP_FREE(soap, tp);
  }
  soap->attributes = NULL;
#ifdef WITH_FAST
  if (soap->labbuf)
    SOAP_FREE(soap, soap->labbuf);
  soap->labbuf = NULL;
  soap->lablen = 0;
  soap->labidx = 0;
#endif
  ns = soap->local_namespaces;
  if (ns)
  { for (; ns->id; ns++)
    { if (ns->out)
      { SOAP_FREE(soap, ns->out);
        ns->out = NULL;
      }
    }
    SOAP_FREE(soap, soap->local_namespaces);
    soap->local_namespaces = NULL;
  }
#ifndef WITH_LEANER
  while (soap->xlist)
  { struct soap_xlist *xp = soap->xlist->next;
    SOAP_FREE(soap, soap->xlist);
    soap->xlist = xp;
  }
#endif
  soap_free_iht(soap);
  soap_free_pht(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_1
static void
soap_free_ns(struct soap *soap)
{ struct soap_nlist *np, *nq;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free namespace stack\n"));
  for (np = soap->nlist; np; np = nq)
  { nq = np->next;
    SOAP_FREE(soap, np);
  }
  soap->nlist = NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
#if !defined(WITH_LEAN) || defined(SOAP_DEBUG)
static void
soap_init_logs(struct soap *soap)
{ int i;
  for (i = 0; i < SOAP_MAXLOGS; i++)
  { soap->logfile[i] = NULL;
    soap->fdebug[i] = NULL;
  }
}
#endif
#endif

/******************************************************************************/
#if !defined(WITH_LEAN) || defined(SOAP_DEBUG)
SOAP_FMAC1
void
SOAP_FMAC2
soap_open_logfile(struct soap *soap, int i)
{ if (soap->logfile[i])
    soap->fdebug[i] = fopen(soap->logfile[i], i < 2 ? "ab" : "a");
}
#endif

/******************************************************************************/
#ifdef SOAP_DEBUG
static void
soap_close_logfile(struct soap *soap, int i)
{ if (soap->fdebug[i])
  { fclose(soap->fdebug[i]);
    soap->fdebug[i] = NULL;
  }
}
#endif

/******************************************************************************/
#ifdef SOAP_DEBUG
SOAP_FMAC1
void
SOAP_FMAC2
soap_close_logfiles(struct soap *soap)
{ int i;
  for (i = 0; i < SOAP_MAXLOGS; i++)
    soap_close_logfile(soap, i);
}
#endif

/******************************************************************************/
#ifdef SOAP_DEBUG
static void
soap_set_logfile(struct soap *soap, int i, const char *logfile)
{ const char *s;
  char *t = NULL;
  soap_close_logfile(soap, i);
  s = soap->logfile[i];
  soap->logfile[i] = logfile;
  if (s)
    SOAP_FREE(soap, s);
  if (logfile)
  { size_t l = strlen(logfile) + 1;
    if ((t = (char*)SOAP_MALLOC(soap, l)))
      soap_memcpy((void*)t, l, (const void*)logfile, l);
  }
  soap->logfile[i] = t;
}
#endif

/******************************************************************************/
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_recv_logfile(struct soap *soap, const char *logfile)
{
  (void)soap; (void)logfile;
#ifdef SOAP_DEBUG
  soap_set_logfile(soap, SOAP_INDEX_RECV, logfile);
#endif
}

/******************************************************************************/
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_sent_logfile(struct soap *soap, const char *logfile)
{
  (void)soap; (void)logfile;
#ifdef SOAP_DEBUG
  soap_set_logfile(soap, SOAP_INDEX_SENT, logfile);
#endif
}

/******************************************************************************/
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_test_logfile(struct soap *soap, const char *logfile)
{
  (void)soap; (void)logfile;
#ifdef SOAP_DEBUG
  soap_set_logfile(soap, SOAP_INDEX_TEST, logfile);
#endif
}

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
struct soap*
SOAP_FMAC2
soap_copy(const struct soap *soap)
{ struct soap *copy = soap_versioning(soap_new)(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
  if (soap_copy_context(copy, soap) != NULL)
    return copy;
  soap_free(copy);
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
struct soap*
SOAP_FMAC2
soap_copy_context(struct soap *copy, const struct soap *soap)
{ if (copy == soap)
    return copy;
  if (soap_check_state(soap))
    return NULL;
  if (copy)
  { struct soap_plugin *p = NULL;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Copying context\n"));
    soap_memcpy((void*)copy, sizeof(struct soap), (const void*)soap, sizeof(struct soap));
    copy->state = SOAP_COPY;
    copy->error = SOAP_OK;
    copy->userid = NULL;
    copy->passwd = NULL;
#ifdef WITH_NTLM
    copy->ntlm_challenge = NULL;
#endif
    copy->nlist = NULL;
    copy->blist = NULL;
    copy->clist = NULL;
    copy->alist = NULL;
    copy->attributes = NULL;
    copy->labbuf = NULL;
    copy->lablen = 0;
    copy->labidx = 0;
#ifdef SOAP_MEM_DEBUG
    soap_init_mht(copy);
#endif
#if !defined(WITH_LEAN) || defined(SOAP_DEBUG)
    soap_init_logs(copy);
#endif
#ifdef SOAP_DEBUG
    soap_set_test_logfile(copy, soap->logfile[SOAP_INDEX_TEST]);
    soap_set_sent_logfile(copy, soap->logfile[SOAP_INDEX_SENT]);
    soap_set_recv_logfile(copy, soap->logfile[SOAP_INDEX_RECV]);
#endif
    copy->namespaces = soap->local_namespaces;
    copy->local_namespaces = NULL;
    soap_set_local_namespaces(copy); /* copy content of soap->local_namespaces */
    copy->namespaces = soap->namespaces; /* point to shared read-only namespaces table */
    copy->c_locale = NULL;
#ifdef WITH_OPENSSL
    copy->bio = NULL;
    copy->ssl = NULL;
    copy->session = NULL;
#endif
#ifdef WITH_GNUTLS
    copy->session = NULL;
#endif
#ifdef WITH_ZLIB
    copy->d_stream = NULL;
    copy->z_buf = NULL;
#endif
    soap_init_iht(copy);
    soap_init_pht(copy);
    copy->header = NULL;
    copy->fault = NULL;
    copy->action = NULL;
#ifndef WITH_LEAN
#ifdef WITH_COOKIES
    copy->cookies = soap_copy_cookies(copy, soap);
#else
    copy->cookies = NULL;
#endif
#endif
    copy->plugins = NULL;
    for (p = soap->plugins; p; p = p->next)
    { struct soap_plugin *q = (struct soap_plugin*)SOAP_MALLOC(copy, sizeof(struct soap_plugin));
      if (!q)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not allocate plugin '%s'\n", p->id));
        soap_end(copy);
        soap_done(copy);
        return NULL;
      }
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Copying plugin '%s'\n", p->id));
      *q = *p;
      if (p->fcopy && (copy->error = p->fcopy(copy, q, p)))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not copy plugin '%s' error = %d\n", p->id, copy->error));
        SOAP_FREE(copy, q);
        soap_end(copy);
        soap_done(copy);
        return NULL;
      }
      q->next = copy->plugins;
      copy->plugins = q;
    }
  }
  return copy;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_copy_stream(struct soap *copy, struct soap *soap)
{ struct soap_attribute *tp = NULL, *tq;
  if (copy == soap)
    return;
  copy->header = soap->header;
  copy->mode = soap->mode;
  copy->imode = soap->imode;
  copy->omode = soap->omode;
  copy->master = soap->master;
  copy->socket = soap->socket;
  copy->sendsk = soap->sendsk;
  copy->recvsk = soap->recvsk;
  copy->recv_timeout = soap->recv_timeout;
  copy->send_timeout = soap->send_timeout;
#if defined(__cplusplus) && !defined(WITH_LEAN)
  copy->os = soap->os;
  copy->is = soap->is;
#endif
  copy->sendfd = soap->sendfd;
  copy->recvfd = soap->recvfd;
  copy->bufidx = soap->bufidx;
  copy->buflen = soap->buflen;
  copy->ahead = soap->ahead;
  copy->cdata = soap->cdata;
  copy->chunksize = soap->chunksize;
  copy->chunkbuflen = soap->chunkbuflen;
  copy->keep_alive = soap->keep_alive;
  copy->tcp_keep_alive = soap->tcp_keep_alive;
  copy->tcp_keep_idle = soap->tcp_keep_idle;
  copy->tcp_keep_intvl = soap->tcp_keep_intvl;
  copy->tcp_keep_cnt = soap->tcp_keep_cnt;
  copy->max_keep_alive = soap->max_keep_alive;
#ifndef WITH_NOIO
  copy->peer = soap->peer;
  copy->peerlen = soap->peerlen;
  copy->ip = soap->ip;
  copy->port = soap->port;
  soap_memcpy((void*)copy->host, sizeof(copy->host), (const void*)soap->host, sizeof(soap->host));
  soap_memcpy((void*)copy->endpoint, sizeof(copy->endpoint), (const void*)soap->endpoint, sizeof(soap->endpoint));
#endif
#ifdef WITH_OPENSSL
  copy->bio = soap->bio;
  copy->ctx = soap->ctx;
  copy->ssl = soap->ssl;
#endif
#ifdef WITH_GNUTLS
  copy->session = soap->session;
#endif
#ifdef WITH_SYSTEMSSL
  copy->ctx = soap->ctx;
  copy->ssl = soap->ssl;
#endif
#ifdef WITH_ZLIB
  copy->zlib_state = soap->zlib_state;
  copy->zlib_in = soap->zlib_in;
  copy->zlib_out = soap->zlib_out;
  if (soap->d_stream && soap->zlib_state != SOAP_ZLIB_NONE)
  { if (!copy->d_stream)
      copy->d_stream = (z_stream*)SOAP_MALLOC(copy, sizeof(z_stream));
    if (copy->d_stream)
      soap_memcpy((void*)copy->d_stream, sizeof(z_stream), (const void*)soap->d_stream, sizeof(z_stream));
  }
  copy->z_crc = soap->z_crc;
  copy->z_ratio_in = soap->z_ratio_in;
  copy->z_ratio_out = soap->z_ratio_out;
  copy->z_buf = NULL;
  copy->z_buflen = soap->z_buflen;
  copy->z_level = soap->z_level;
  if (soap->z_buf && soap->zlib_state != SOAP_ZLIB_NONE)
  { copy->z_buf = (char*)SOAP_MALLOC(copy, sizeof(soap->buf));
    if (copy->z_buf)
      soap_memcpy((void*)copy->z_buf, sizeof(soap->buf), (const void*)soap->z_buf, sizeof(soap->buf));
  }
  copy->z_dict = soap->z_dict;
  copy->z_dict_len = soap->z_dict_len;
#endif
  soap_memcpy((void*)copy->buf, sizeof(copy->buf), (const void*)soap->buf, sizeof(soap->buf));
  /* copy XML parser state */
  soap_free_ns(copy);
  soap_set_local_namespaces(copy);
  copy->version = soap->version;
  if (soap->nlist && soap->local_namespaces)
  { struct soap_nlist *np = NULL, *nq;
    /* copy reversed nlist */
    for (nq = soap->nlist; nq; nq = nq->next)
    { struct soap_nlist *nr = np;
      size_t n = sizeof(struct soap_nlist) + strlen(nq->id);
      np = (struct soap_nlist*)SOAP_MALLOC(copy, n);
      if (!np)
        break;
      soap_memcpy((void*)np, n, (const void*)nq, n);
      np->next = nr;
    }
    while (np)
    { const char *s = np->ns;
      copy->level = np->level; /* preserve element nesting level */
      if (!s && np->index >= 0)
      { s = soap->local_namespaces[np->index].out;
        if (!s)
          s = soap->local_namespaces[np->index].ns;
      }
      if (s && soap_push_namespace(copy, np->id, s) == NULL)
        break;
      nq = np;
      np = np->next;
      SOAP_FREE(copy, nq);
    }
  }
  soap_memcpy((void*)copy->tag, sizeof(copy->tag), (const void*)soap->tag, sizeof(soap->tag));
  soap_memcpy((void*)copy->id, sizeof(copy->id), (const void*)soap->id, sizeof(soap->id));
  soap_memcpy((void*)copy->href, sizeof(copy->href), (const void*)soap->href, sizeof(soap->href));
  soap_memcpy((void*)copy->type, sizeof(copy->type), (const void*)soap->type, sizeof(soap->type));
  copy->other = soap->other;
  copy->root = soap->root;
  copy->null = soap->null;
  copy->body = soap->body;
  copy->part = soap->part;
  copy->mustUnderstand = soap->mustUnderstand;
  copy->level = soap->level;
  copy->peeked = soap->peeked;
  /* copy attributes */
  for (tq = soap->attributes; tq; tq = tq->next)
  { struct soap_attribute *tr = tp;
    size_t n = sizeof(struct soap_attribute) + strlen(tq->name);
    tp = (struct soap_attribute*)SOAP_MALLOC(copy, n);
    soap_memcpy((void*)tp, n, (const void*)tq, n);
    if (tp->size)
    { tp->value = (char*)SOAP_MALLOC(copy, tp->size);
      if (tp->value)
        soap_memcpy((void*)tp->value, tp->size, (const void*)tq->value, tp->size);
    }
    tp->ns = NULL;
    tp->next = tr;
  }
  copy->attributes = tp;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_free_stream(struct soap *soap)
{ soap->socket = SOAP_INVALID_SOCKET;
  soap->sendsk = SOAP_INVALID_SOCKET;
  soap->recvsk = SOAP_INVALID_SOCKET;
#ifdef WITH_OPENSSL
  soap->bio = NULL;
  soap->ctx = NULL;
  soap->ssl = NULL;
#endif
#ifdef WITH_GNUTLS
  soap->xcred = NULL;
  soap->acred = NULL;
  soap->cache = NULL;
  soap->session = NULL;
  soap->dh_params = NULL;
  soap->rsa_params = NULL;
#endif
#ifdef WITH_SYSTEMSSL
  soap->ctx = (gsk_handle)NULL;
  soap->ssl = (gsk_handle)NULL;
#endif
#ifdef WITH_ZLIB
  if (soap->z_buf)
    SOAP_FREE(soap, soap->z_buf);
  soap->z_buf = NULL;
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_initialize(struct soap *soap)
{ soap_versioning(soap_init)(soap, SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_versioning(soap_init)(struct soap *soap, soap_mode imode, soap_mode omode)
{ size_t i;
  soap->state = SOAP_INIT;
#ifdef SOAP_MEM_DEBUG
  soap_init_mht(soap);
#endif
#if !defined(WITH_LEAN) || defined(SOAP_DEBUG)
  soap_init_logs(soap);
#endif
#ifdef SOAP_DEBUG
#ifdef TANDEM_NONSTOP
  soap_set_test_logfile(soap, "TESTLOG");
  soap_set_sent_logfile(soap, "SENTLOG");
  soap_set_recv_logfile(soap, "RECVLOG");
#else
  soap_set_test_logfile(soap, "TEST.log");
  soap_set_sent_logfile(soap, "SENT.log");
  soap_set_recv_logfile(soap, "RECV.log");
#endif
#endif
  soap->version = 0;
  soap_mode(soap, imode);
  soap_imode(soap, imode);
  soap_omode(soap, omode);
  soap->plugins = NULL;
  soap->user = NULL;
  for (i = 0; i < sizeof(soap->data)/sizeof(*soap->data); i++)
    soap->data[i] = NULL;
  soap->userid = NULL;
  soap->passwd = NULL;
  soap->authrealm = NULL;
#ifdef WITH_NTLM
  soap->ntlm_challenge = NULL;
#endif
#ifndef WITH_NOHTTP
  soap->fpost = http_post;
  soap->fget = http_get;
  soap->fput = http_405;
  soap->fdel = http_405;
  soap->fopt = http_200;
  soap->fhead = http_200;
  soap->fform = NULL;
  soap->fposthdr = http_post_header;
  soap->fresponse = http_response;
  soap->fparse = http_parse;
  soap->fparsehdr = http_parse_header;
#endif
  soap->fheader = NULL;
  soap->fconnect = NULL;
  soap->fdisconnect = NULL;
#ifndef WITH_NOIO
  soap->ipv6_multicast_if = 0; /* in_addr_t value */
  soap->ipv4_multicast_if = NULL; /* points to struct in_addr or in_addr_t */
  soap->ipv4_multicast_ttl = 0; /* 0: use default */
#ifndef WITH_IPV6
  soap->fresolve = tcp_gethost;
#else
  soap->fresolve = NULL;
#endif
  soap->faccept = tcp_accept;
  soap->fopen = tcp_connect;
  soap->fclose = tcp_disconnect;
  soap->fclosesocket = tcp_closesocket;
  soap->fshutdownsocket = tcp_shutdownsocket;
  soap->fsend = fsend;
  soap->frecv = frecv;
  soap->fpoll = soap_poll;
#else
  soap->fopen = NULL;
  soap->fclose = NULL;
  soap->fpoll = NULL;
#endif
  soap->fseterror = NULL;
  soap->fignore = NULL;
  soap->fserveloop = NULL;
  soap->fplugin = fplugin;
  soap->fmalloc = NULL;
#ifndef WITH_LEANER
  soap->fsvalidate = NULL;
  soap->fwvalidate = NULL;
  soap->feltbegin = NULL;
  soap->feltendin = NULL;
  soap->feltbegout = NULL;
  soap->feltendout = NULL;
  soap->fprepareinitsend = NULL;
  soap->fprepareinitrecv = NULL;
  soap->fpreparesend = NULL;
  soap->fpreparerecv = NULL;
  soap->fpreparefinalsend = NULL;
  soap->fpreparefinalrecv = NULL;
  soap->ffiltersend = NULL;
  soap->ffilterrecv = NULL;
  soap->fdimereadopen = NULL;
  soap->fdimewriteopen = NULL;
  soap->fdimereadclose = NULL;
  soap->fdimewriteclose = NULL;
  soap->fdimeread = NULL;
  soap->fdimewrite = NULL;
  soap->fmimereadopen = NULL;
  soap->fmimewriteopen = NULL;
  soap->fmimereadclose = NULL;
  soap->fmimewriteclose = NULL;
  soap->fmimeread = NULL;
  soap->fmimewrite = NULL;
#endif
  soap->float_format = "%.9G"; /* Alternative: use "%G" */
  soap->double_format = "%.17lG"; /* Alternative: use "%lG" */
  soap->long_double_format = NULL; /* Defined in custom serializer custom/long_double.c */
  soap->dime_id_format = "cid:id%d"; /* default DIME id format for int id index */
  soap->http_version = "1.1";
  soap->proxy_http_version = "1.0";
  soap->http_content = NULL;
  soap->actor = NULL;
  soap->lang = "en";
  soap->keep_alive = 0;
  soap->tcp_keep_alive = 0;
  soap->tcp_keep_idle = 0;
  soap->tcp_keep_intvl = 0;
  soap->tcp_keep_cnt = 0;
  soap->max_keep_alive = SOAP_MAXKEEPALIVE;
  soap->recv_timeout = 0;
  soap->send_timeout = 0;
  soap->connect_timeout = 0;
  soap->accept_timeout = 0;
  soap->socket_flags = 0;
  soap->connect_flags = 0;
  soap->bind_flags = 0;
  soap->accept_flags = 0;
  soap->linger_time = 0;
  soap->ip = 0;
  soap->labbuf = NULL;
  soap->lablen = 0;
  soap->labidx = 0;
  soap->encodingStyle = NULL;
#ifndef WITH_NONAMESPACES
  soap->namespaces = namespaces;
#else
  soap->namespaces = NULL;
#endif
  soap->local_namespaces = NULL;
  soap->nlist = NULL;
  soap->blist = NULL;
  soap->clist = NULL;
  soap->alist = NULL;
  soap->shaky = 0;
  soap->attributes = NULL;
  soap->header = NULL;
  soap->fault = NULL;
  soap->master = SOAP_INVALID_SOCKET;
  soap->socket = SOAP_INVALID_SOCKET;
  soap->sendsk = SOAP_INVALID_SOCKET;
  soap->recvsk = SOAP_INVALID_SOCKET;
  soap->os = NULL;
  soap->is = NULL;
#ifndef WITH_LEANER
  soap->dom = NULL;
  soap->dime.list = NULL;
  soap->dime.first = NULL;
  soap->dime.last = NULL;
  soap->mime.list = NULL;
  soap->mime.first = NULL;
  soap->mime.last = NULL;
  soap->mime.boundary = NULL;
  soap->mime.start = NULL;
  soap->xlist = NULL;
#endif
#ifndef UNDER_CE
  soap->recvfd = 0;
  soap->sendfd = 1;
#else
  soap->recvfd = stdin;
  soap->sendfd = stdout;
#endif
  soap->host[0] = '\0';
  soap->path[0] = '\0';
  soap->port = 0;
  soap->action = NULL;
  soap->proxy_host = NULL;
  soap->proxy_port = 8080;
  soap->proxy_userid = NULL;
  soap->proxy_passwd = NULL;
  soap->prolog = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
#ifdef WITH_ZLIB
  soap->zlib_state = SOAP_ZLIB_NONE;
  soap->zlib_in = SOAP_ZLIB_NONE;
  soap->zlib_out = SOAP_ZLIB_NONE;
  soap->d_stream = NULL;
  soap->z_buf = NULL;
  soap->z_level = 6;
  soap->z_dict = NULL;
  soap->z_dict_len = 0;
#endif
#ifndef WITH_LEAN
  soap->wsuid = NULL;
  soap->c14nexclude = NULL;
  soap->c14ninclude = NULL;
  soap->cookies = NULL;
  soap->cookie_domain = NULL;
  soap->cookie_path = NULL;
  soap->cookie_max = 32;
#endif
#ifdef WMW_RPM_IO
  soap->rpmreqid = NULL;
#endif
#ifdef PALM
  palmNetLibOpen();
#endif
#ifndef WITH_NOIDREF
  soap_init_iht(soap);
#endif
  soap_init_pht(soap);
#ifdef WITH_OPENSSL
  if (!soap_ssl_init_done)
    soap_ssl_init();
  soap->fsslauth = ssl_auth_init;
  soap->fsslverify = NULL;
  soap->bio = NULL;
  soap->ssl = NULL;
  soap->ctx = NULL;
  soap->session = NULL;
  soap->ssl_flags = SOAP_SSL_DEFAULT;
  soap->keyfile = NULL;
  soap->keyid = NULL;
  soap->password = NULL;
  soap->cafile = NULL;
  soap->capath = NULL;
  soap->crlfile = NULL;
  soap->dhfile = NULL;
  soap->randfile = NULL;
#endif
#ifdef WITH_GNUTLS
  if (!soap_ssl_init_done)
    soap_ssl_init();
  soap->fsslauth = ssl_auth_init;
  soap->fsslverify = NULL;
  soap->xcred = NULL;
  soap->acred = NULL;
  soap->cache = NULL;
  soap->session = NULL;
  soap->ssl_flags = SOAP_SSL_DEFAULT;
  soap->keyfile = NULL;
  soap->keyid = NULL;
  soap->password = NULL;
  soap->cafile = NULL;
  soap->capath = NULL;
  soap->crlfile = NULL;
  soap->dh_params = NULL;
  soap->rsa_params = NULL;
#endif
#ifdef WITH_SYSTEMSSL
  soap->fsslauth = ssl_auth_init;
  soap->fsslverify = NULL;
  soap->bio = NULL;
  soap->ssl = (gsk_handle)NULL;
  soap->ctx = (gsk_handle)NULL;
  soap->session = NULL;
  soap->ssl_flags = SOAP_SSL_DEFAULT;
  soap->keyfile = NULL;
  soap->keyid = NULL;
  soap->password = NULL;
  soap->cafile = NULL;
  soap->capath = NULL;
  soap->crlfile = NULL;
  soap->dhfile = NULL;
  soap->randfile = NULL;
#endif
  soap->c_locale = NULL;
  soap->buflen = 0;
  soap->bufidx = 0;
#ifndef WITH_LEANER
  soap->dime.chunksize = 0;
  soap->dime.buflen = 0;
#endif
  soap->null = 0;
  soap->position = 0;
  soap->encoding = 0;
  soap->mustUnderstand = 0;
  soap->ns = 0;
  soap->part = SOAP_END;
  soap->event = 0;
  soap->evlev = 0;
  soap->alloced = 0;
  soap->count = 0;
  soap->length = 0;
  soap->cdata = 0;
  soap->peeked = 0;
  soap->ahead = 0;
  soap->idnum = 0;
  soap->level = 0;
  soap->endpoint[0] = '\0';
  soap->error = SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_begin(struct soap *soap)
{ if (soap_check_state(soap))
    return;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Reinitializing context\n"));
  if (!soap->keep_alive)
  { soap->buflen = 0;
    soap->bufidx = 0;
  }
  soap->null = 0;
  soap->position = 0;
  soap->encoding = 0;
  soap->mustUnderstand = 0;
  soap->mode = 0;
  soap->ns = 0;
  soap->part = SOAP_END;
  soap->event = 0;
  soap->evlev = 0;
  soap->count = 0;
  soap->length = 0;
  soap->cdata = 0;
  soap->error = SOAP_OK;
  soap->peeked = 0;
  soap->ahead = 0;
  soap->idnum = 0;
  soap->level = 0;
  soap->endpoint[0] = '\0';
  soap->encodingStyle = SOAP_STR_EOS;
#ifndef WITH_LEANER
  soap->dime.chunksize = 0;
  soap->dime.buflen = 0;
#endif
  soap_free_temp(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_end(struct soap *soap)
{ if (soap_check_state(soap))
    return;
  soap_free_temp(soap);
  soap_dealloc(soap, NULL);
  while (soap->clist)
  { struct soap_clist *cp = soap->clist->next;
    SOAP_FREE(soap, soap->clist);
    soap->clist = cp;
  }
  soap_closesock(soap);
#ifdef SOAP_DEBUG
  soap_close_logfiles(soap);
#endif
#ifdef PALM
  palmNetLibClose();
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_version(struct soap *soap, short version)
{ soap_set_local_namespaces(soap);
  if (soap->version != version)
  { if (version == 1)
    { soap->local_namespaces[0].ns = soap_env1;
      soap->local_namespaces[1].ns = soap_enc1;
    }
    else if (version == 2)
    { soap->local_namespaces[0].ns = soap_env2;
      soap->local_namespaces[1].ns = soap_enc2;
    }
    soap->version = version;
  }
  if (version == 0)
    soap->encodingStyle = SOAP_STR_EOS;
  else
    soap->encodingStyle = NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_get_version(struct soap *soap)
{ struct Namespace *p = soap->local_namespaces;
  if (p)
  { const char *ns = p[0].out;
    if (!ns)
      ns = p[0].ns;
    if (!strcmp(ns, soap_env1))
    { soap->version = 1; /* make sure we use SOAP 1.1 */
      if (p[1].out)
        SOAP_FREE(soap, p[1].out);
      if ((p[1].out = (char*)SOAP_MALLOC(soap, sizeof(soap_enc1))))
        soap_strncpy(p[1].out, sizeof(soap_enc1), soap_enc1, sizeof(soap_enc1) - 1);
    }
    else if (!strcmp(ns, soap_env2))
    { soap->version = 2; /* make sure we use SOAP 1.2 */
      if (p[1].out)
        SOAP_FREE(soap, p[1].out);
      if ((p[1].out = (char*)SOAP_MALLOC(soap, sizeof(soap_enc2))))
        soap_strncpy(p[1].out, sizeof(soap_enc2), soap_enc2, sizeof(soap_enc2) - 1);
    }
  }
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_namespaces(struct soap *soap, const struct Namespace *p)
{ struct Namespace *ns = soap->local_namespaces;
  struct soap_nlist *np, *nq, *nr;
  unsigned int level = soap->level;
  soap->namespaces = p;
  soap->local_namespaces = NULL;
  soap_set_local_namespaces(soap);
  /* reverse the namespace list */
  np = soap->nlist;
  soap->nlist = NULL;
  if (np)
  { nq = np->next;
    np->next = NULL;
    while (nq)
    { nr = nq->next;
      nq->next = np;
      np = nq;
      nq = nr;
    }
  }
  /* then push on new stack */
  while (np)
  { const char *s;
    soap->level = np->level; /* preserve element nesting level */
    s = np->ns;
    if (!s && np->index >= 0 && ns)
    { s = ns[np->index].out;
      if (!s)
        s = ns[np->index].ns;
    }
    if (s && soap_push_namespace(soap, np->id, s) == NULL)
      return soap->error;
    nq = np;
    np = np->next;
    SOAP_FREE(soap, nq);
  }
  if (ns)
  { int i;
    for (i = 0; ns[i].id; i++)
    { if (ns[i].out)
      { SOAP_FREE(soap, ns[i].out);
        ns[i].out = NULL;
      }
    }
    SOAP_FREE(soap, ns);
  }
  soap->level = level; /* restore level */
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_local_namespaces(struct soap *soap)
{ if (soap->namespaces && !soap->local_namespaces)
  { const struct Namespace *ns1;
    struct Namespace *ns2;
    size_t n = 1;
    for (ns1 = soap->namespaces; ns1->id; ns1++)
      n++;
    n *= sizeof(struct Namespace);
    ns2 = (struct Namespace*)SOAP_MALLOC(soap, n);
    if (ns2)
    { soap_memcpy((void*)ns2, n, (const void*)soap->namespaces, n);
      if (ns2[0].ns)
      { if (!strcmp(ns2[0].ns, soap_env1))
          soap->version = 1;
        else if (!strcmp(ns2[0].ns, soap_env2))
          soap->version = 2;
      }
      soap->local_namespaces = ns2;
      for (; ns2->id; ns2++)
        ns2->out = NULL;
    }
  }
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifndef PALM_1
SOAP_FMAC1
const char *
SOAP_FMAC2
soap_tagsearch(const char *big, const char *little)
{ if (little)
  { size_t n = strlen(little);
    const char *s = big;
    while (s)
    { const char *t = s;
      size_t i;
      for (i = 0; i < n; i++, t++)
      { if (*t != little[i])
          break;
      }
      if (*t == '\0' || *t == ' ')
      { if (i == n || (i && little[i-1] == ':'))
          return s;
      }
      s = strchr(t, ' ');
      if (s)
        s++;
    }
  }
  return NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifndef PALM_1
SOAP_FMAC1
struct soap_nlist *
SOAP_FMAC2
soap_lookup_ns(struct soap *soap, const char *tag, size_t n)
{ struct soap_nlist *np;
  for (np = soap->nlist; np; np = np->next)
  { if (!strncmp(np->id, tag, n) && !np->id[n])
      return np;
  }
  return NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
static struct soap_nlist *
soap_push_ns(struct soap *soap, const char *id, const char *ns, short utilized)
{ struct soap_nlist *np;
  size_t n, k;
  if (soap_tagsearch(soap->c14nexclude, id))
    return NULL;
  if (!utilized)
  { for (np = soap->nlist; np; np = np->next)
    { if (!strcmp(np->id, id) && (!np->ns || !strcmp(np->ns, ns)))
        break;
    }
    if (np)
    { if ((np->level < soap->level || !np->ns) && np->index == 1)
        utilized = 1;
      else
        return NULL;
    }
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Adding namespace binding (level=%u) '%s' '%s' utilized=%d\n", soap->level, id, ns ? ns : "(null)", utilized));
  n = strlen(id);
  if (ns)
    k = strlen(ns);
  else
    k = 0;
  np = (struct soap_nlist*)SOAP_MALLOC(soap, sizeof(struct soap_nlist) + n + k + 1);
  if (!np)
  { soap->error = SOAP_EOM;
    return NULL;
  }
  np->next = soap->nlist;
  soap->nlist = np;
  soap_strcpy((char*)np->id, n + 1, id);
  if (ns)
  { np->ns = np->id + n + 1;
    soap_strcpy((char*)np->ns, k + 1, ns);
  }
  else
    np->ns = NULL;
  np->level = soap->level;
  np->index = utilized;
  return np;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
static void
soap_utilize_ns(struct soap *soap, const char *tag)
{ struct soap_nlist *np;
  size_t n = 0;
  const char *t = strchr(tag, ':');
  if (t)
  { n = t - tag;
    if (n >= sizeof(soap->tmpbuf))
      n = sizeof(soap->tmpbuf) - 1;
  }
  np = soap_lookup_ns(soap, tag, n);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Utilizing namespace of '%s'\n", tag));
  if (np)
  { if (np->index <= 0)
      soap_push_ns(soap, np->id, np->ns, 1);
  }
  else if (strncmp(tag, "xml", 3))
  { soap_strncpy(soap->tmpbuf, sizeof(soap->tmpbuf), tag, n);
    soap_push_ns(soap, soap->tmpbuf, NULL, 1);
  }
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_element(struct soap *soap, const char *tag, int id, const char *type)
{
#ifndef WITH_LEAN
  const char *s;
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Element begin tag='%s' level='%u' id='%d' type='%s'\n", tag, soap->level, id, type ? type : SOAP_STR_EOS));
  soap->level++;
#ifdef WITH_DOM
#ifndef WITH_LEAN
  if (soap->wsuid && soap_tagsearch(soap->wsuid, tag))
  { size_t i;
    for (s = tag, i = 0; *s && i < sizeof(soap->tag) - 1; s++, i++)
      soap->tag[i] = *s == ':' ? '-' : *s;
    soap->tag[i] = '\0';
    if (soap_set_attr(soap, "wsu:Id", soap->tag, 1))
      return soap->error;
  }
  if ((soap->mode & SOAP_XML_CANONICAL) && !(soap->mode & SOAP_DOM_ASIS))
  { if (soap->evlev >= soap->level)
      soap->evlev = 0;
    if (soap->event == SOAP_SEC_BEGIN && !soap->evlev)
    { struct soap_nlist *np;
      /* non-nested wsu:Id found: clear xmlns, re-emit them for exc-c14n */
      for (np = soap->nlist; np; np = np->next)
      { int p = soap->c14ninclude && soap_tagsearch(soap->c14ninclude, np->id);
        if (np->index == 2 || p)
        { struct soap_nlist *np1 = soap_push_ns(soap, np->id, np->ns, 1);
	  if (np1 && !p)
            np1->index = 0;
        }
      }
      soap->evlev = soap->level;
    }
  }
#endif
  if (soap->mode & SOAP_XML_DOM)
  { struct soap_dom_element *elt = (struct soap_dom_element*)soap_malloc(soap, sizeof(struct soap_dom_element));
    if (!elt)
      return soap->error;
    elt->soap = soap;
    elt->next = NULL;
    elt->prnt = soap->dom;
    elt->name = soap_strdup(soap, tag);
    elt->elts = NULL;
    elt->atts = NULL;
    elt->nstr = NULL;
    elt->data = NULL;
    elt->wide = NULL;
    elt->node = NULL;
    elt->type = 0;
    elt->head = NULL;
    elt->tail = NULL;
    if (soap->dom)
    { struct soap_dom_element *p = soap->dom->elts;
      if (p)
      { while (p->next)
          p = p->next;
        p->next = elt;
      }
      else
        soap->dom->elts = elt;
    }
    soap->dom = elt;
  }
  else
  {
#endif
#ifndef WITH_LEAN
    if (!soap->ns)
    { if (!(soap->mode & SOAP_XML_CANONICAL) && soap_send(soap, soap->prolog))
        return soap->error;
    }
    else if (soap->mode & SOAP_XML_INDENT)
    { if (soap->ns == 1 && soap_send_raw(soap, soap_indent, soap->level < sizeof(soap_indent) ? soap->level : sizeof(soap_indent) - 1))
        return soap->error;
      soap->body = 1;
    }
    if ((soap->mode & SOAP_XML_DEFAULTNS) && (s = strchr(tag, ':')))
    { struct Namespace *ns = soap->local_namespaces;
      size_t n = s - tag;
      if (soap_send_raw(soap, "<", 1)
       || soap_send(soap, s + 1))
        return soap->error;
      if (soap->nlist && !strncmp(soap->nlist->id, tag, n) && !soap->nlist->id[n])
        ns = NULL;
      for (; ns && ns->id; ns++)
      { if (*ns->id && (ns->out || ns->ns) && !strncmp(ns->id, tag, n) && !ns->id[n])
        { soap_push_ns(soap, ns->id, ns->out ? ns->out : ns->ns, 0);
          if (soap_attribute(soap, "xmlns", ns->out ? ns->out : ns->ns))
            return soap->error;
          break;
        }
      }
    }
    else
#endif
    if (soap_send_raw(soap, "<", 1)
     || soap_send(soap, tag))
      return soap->error;
#ifdef WITH_DOM
  }
#endif
  if (!soap->ns)
  { struct Namespace *ns = soap->local_namespaces;
    int k = -1;
    if (ns)
    {
#ifndef WITH_LEAN
      if ((soap->mode & SOAP_XML_DEFAULTNS))
      { if (soap->version)
          k = 4; /* first four required entries */
        else if (!(soap->mode & SOAP_XML_NOTYPE) || (soap->mode & SOAP_XML_NIL))
        { ns += 2;
          k = 2; /* next two entries */
        }
        else
          k = 0; /* no entries */
      }
#endif
      while (k-- && ns->id)
      { const char *t = ns->out;
        if (!t)
          t = ns->ns;
        if (*ns->id && t && *t)
        { (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), strlen(ns->id) + 6), "xmlns:%s", ns->id);
          if (soap_attribute(soap, soap->tmpbuf, t))
            return soap->error;
        }
        ns++;
      }
    }
  }
  soap->ns = 1; /* namespace table control: ns = 0 or 2 to start, then 1 to stop dumping the table  */
#ifndef WITH_LEAN
  if (soap->mode & SOAP_XML_CANONICAL)
    soap_utilize_ns(soap, tag);
#endif
  if (id > 0)
  { (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), sizeof(SOAP_BASEREFNAME) + 20), SOAP_BASEREFNAME "%d", id);
    if (soap->version == 2)
    { if (soap_attribute(soap, "SOAP-ENC:id", soap->tmpbuf))
        return soap->error;
    }
    else if (soap_attribute(soap, "id", soap->tmpbuf))
      return soap->error;
  }
  if (type && *type && !(soap->mode & SOAP_XML_NOTYPE) && soap->part != SOAP_IN_HEADER)
  { const char *t = type;
#ifndef WITH_LEAN
    if (soap->mode & SOAP_XML_DEFAULTNS)
    { t = strchr(type, ':');
      if (t)
        t++;
      else
        t = type;
    }
    else if (soap->mode & SOAP_XML_CANONICAL)
      soap_utilize_ns(soap, type);
#endif
    if (soap->attributes ? soap_set_attr(soap, "xsi:type", t, 1) : soap_attribute(soap, "xsi:type", t))
      return soap->error;
  }
  if (soap->null && soap->position > 0 && soap->version == 1)
  { int i;
    (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf) - 1, 20), "[%d", soap->positions[0]);
    for (i = 1; i < soap->position; i++)
    { size_t l = strlen(soap->tmpbuf);
      (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l - 1, 20), ",%d", soap->positions[i]);
    }
    soap_strncat(soap->tmpbuf, sizeof(soap->tmpbuf), "]", 1);
    if (soap_attribute(soap, "SOAP-ENC:position", soap->tmpbuf))
      return soap->error;
  }
  if (soap->mustUnderstand)
  { if (soap->actor && *soap->actor)
    { if (soap_attribute(soap, soap->version == 2 ? "SOAP-ENV:role" : "SOAP-ENV:actor", soap->actor))
        return soap->error;
    }
    if (soap_attribute(soap, "SOAP-ENV:mustUnderstand", soap->version == 2 ? "true" : "1"))
      return soap->error;
    soap->mustUnderstand = 0;
  }
  if (soap->encoding)
  { if (soap->encodingStyle && soap->local_namespaces && soap->local_namespaces[0].id && soap->local_namespaces[1].id)
    { if (!*soap->encodingStyle)
      { if (soap->local_namespaces[1].out)
          soap->encodingStyle = soap->local_namespaces[1].out;
        else
          soap->encodingStyle = soap->local_namespaces[1].ns;
      }
      if (soap->encodingStyle && soap_attribute(soap, "SOAP-ENV:encodingStyle", soap->encodingStyle))
        return soap->error;
    }
    else
      soap->encodingStyle = NULL;
    soap->encoding = 0;
  }
  soap->null = 0;
  soap->position = 0;
  if (soap->event == SOAP_SEC_BEGIN)
    soap->event = 0;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_begin_out(struct soap *soap, const char *tag, int id, const char *type)
{ if (*tag == '-')
    return SOAP_OK;
  if (soap_element(soap, tag, id, type))
    return soap->error;
#ifdef WITH_DOM
  if (soap_element_start_end_out(soap, NULL))
    return soap->error;
  if (soap->feltbegout)
    return soap->error = soap->feltbegout(soap, tag);
  return SOAP_OK;
#else
  return soap_element_start_end_out(soap, NULL);
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_2
#ifndef HAVE_STRRCHR
SOAP_FMAC1
char*
SOAP_FMAC2
soap_strrchr(const char *s, int t)
{ char *r = NULL;
  while (*s)
    if (*s++ == t)
      r = (char*)s - 1;
  return r;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
#ifndef HAVE_STRTOL
SOAP_FMAC1
long
SOAP_FMAC2
soap_strtol(const char *s, char **t, int b)
{ long n = 0;
  int c;
  while (*s > 0 && *s <= 32)
    s++;
  if (b == 10)
  { short neg = 0;
    if (*s == '-')
    { s++;
      neg = 1;
    }
    else if (*s == '+')
      s++;
    while ((c = *s) && c >= '0' && c <= '9')
    { if (n >= 214748364 && (n > 214748364 || c >= '8'))
      { if (neg && n == 214748364 && c == '8')
	{ if (t)
	    *t = (char*)(s + 1);
	  return -2147483648;
	}
        break;
      }
      n *= 10;
      n += c - '0';
      s++;
    }
    if (neg)
      n = -n;
  }
  else /* assume b == 16 and value is always positive */
  { while ((c = *s))
    { if (c >= '0' && c <= '9')
        c -= '0';
      else if (c >= 'A' && c <= 'F')
        c -= 'A' - 10;
      else if (c >= 'a' && c <= 'f')
        c -= 'a' - 10;
      if (n > 0x07FFFFFF)
        break;
      n <<= 4;
      n += c;
      s++;
    }
  }
  if (t)
    *t = (char*)s;
  return n;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
#ifndef HAVE_STRTOUL
SOAP_FMAC1
unsigned long
SOAP_FMAC2
soap_strtoul(const char *s, char **t, int b)
{ unsigned long n = 0;
  int c;
  while (*s > 0 && *s <= 32)
    s++;
  if (b == 10)
  { if (*s == '+')
      s++;
    while ((c = *s) && c >= '0' && c <= '9')
    { if (n >= 429496729 && (n > 429496729 || c >= '6'))
        break;
      n *= 10;
      n += c - '0';
      s++;
    }
  }
  else /* b == 16 */
  { while ((c = *s))
    { if (c >= '0' && c <= '9')
        c -= '0';
      else if (c >= 'A' && c <= 'F')
        c -= 'A' - 10;
      else if (c >= 'a' && c <= 'f')
        c -= 'a' - 10;
      if (n > 0x0FFFFFFF)
        break;
      n <<= 4;
      n += c;
      s++;
    }
  }
  if (t)
    *t = (char*)s;
  return n;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_array_begin_out(struct soap *soap, const char *tag, int id, const char *type, const char *offset)
{ if (!type || !*type || soap->version == 0)
    return soap_element_begin_out(soap, tag, id, NULL);
  if (soap_element(soap, tag, id, NULL))
    return soap->error;
  if (soap->version == 1)
  { if (offset && soap_attribute(soap, "SOAP-ENC:offset", offset))
      return soap->error;
    if (soap_attribute(soap, "SOAP-ENC:arrayType", type))
      return soap->error;
  }
  else
  { const char *s;
    s = soap_strrchr(type, '[');
    if (s && (size_t)(s - type) < sizeof(soap->tmpbuf))
    { soap_strncpy(soap->tmpbuf, sizeof(soap->tmpbuf), type, s - type);
      if (soap_attribute(soap, "SOAP-ENC:itemType", soap->tmpbuf))
        return soap->error;
      s++;
      if (*s)
      { soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), s);
        soap->tmpbuf[strlen(soap->tmpbuf) - 1] = '\0';
        if (soap_attribute(soap, "SOAP-ENC:arraySize", soap->tmpbuf))
          return soap->error;
      }
    }
  }
#ifndef WITH_LEAN
  if ((soap->mode & SOAP_XML_CANONICAL))
    soap_utilize_ns(soap, type);
#endif
  return soap_element_start_end_out(soap, NULL);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_start_end_out(struct soap *soap, const char *tag)
{ struct soap_attribute *tp;
#ifndef WITH_LEAN
  if (soap->mode & SOAP_XML_CANONICAL)
  { struct soap_nlist *np;
    for (tp = soap->attributes; tp; tp = tp->next)
    { if (tp->visible && *tp->name)
        soap_utilize_ns(soap, tp->name);
    }
    for (np = soap->nlist; np; np = np->next)
    { if (np->ns && (np->index == 1 || (np->index == 0 && soap->c14ninclude && soap_tagsearch(soap->c14ninclude, np->id))))
      { if (*(np->id))
          (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), strlen(np->id) + 6), "xmlns:%s", np->id);
        else
          soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), "xmlns");
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Enabling utilized binding (level=%u) %s='%s'\n", np->level, soap->tmpbuf, np->ns));
        soap_set_attr(soap, soap->tmpbuf, np->ns, 1);
        np->index = 2;
      }
    }
  }
#endif
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { struct soap_dom_attribute **att;
    att = &soap->dom->atts;
    for (tp = soap->attributes; tp; tp = tp->next)
    { if (tp->visible)
      { *att = (struct soap_dom_attribute*)soap_malloc(soap, sizeof(struct soap_dom_attribute));
        if (!*att)
          return soap->error;
        (*att)->next = NULL;
        (*att)->nstr = NULL;
        (*att)->name = soap_strdup(soap, tp->name);
        (*att)->data = soap_strdup(soap, tp->value);
        (*att)->wide = NULL;
        (*att)->soap = soap;
        att = &(*att)->next;
        tp->visible = 0;
      }
    }
    return SOAP_OK;
  }
#endif
  for (tp = soap->attributes; tp; tp = tp->next)
  { if (tp->visible)
    {
#ifndef WITH_LEAN
      const char *s;
      if ((soap->mode & SOAP_XML_DEFAULTNS) && (s = strchr(tp->name, ':')))
      { size_t n = s - tp->name;
        if (soap->nlist && !strncmp(soap->nlist->id, tp->name, n) && !soap->nlist->id[n])
          s++;
        else
          s = tp->name;
        if (soap_send_raw(soap, " ", 1) || soap_send(soap, s))
          return soap->error;
      }
      else
#endif
      if (soap_send_raw(soap, " ", 1) || soap_send(soap, tp->name))
        return soap->error;
      if (tp->visible == 2 && tp->value)
      { if (soap_send_raw(soap, "=\"", 2)
         || soap_string_out(soap, tp->value, tp->flag)
         || soap_send_raw(soap, "\"", 1))
          return soap->error;
      }
      else if (soap->mode & SOAP_XML_STRICT)
      { if (soap_send_raw(soap, "=\"\"", 3))
          return soap->error;
      }
      tp->visible = 0;
    }
  }
  if (tag)
  {
#ifndef WITH_LEAN
    if (soap->mode & SOAP_XML_CANONICAL)
    { if (soap_send_raw(soap, ">", 1)
       || soap_element_end_out(soap, tag))
        return soap->error;
      return SOAP_OK;
    }
#endif
    soap->level--;	/* decrement level just before /> */
    return soap_send_raw(soap, "/>", 2);
  }
  return soap_send_raw(soap, ">", 1);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_end_out(struct soap *soap, const char *tag)
{
#ifndef WITH_LEAN
  const char *s;
#endif
  if (*tag == '-')
    return SOAP_OK;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Element ending tag='%s'\n", tag));
#ifdef WITH_DOM
  if (soap->feltendout && (soap->error = soap->feltendout(soap, tag)))
    return soap->error;
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { if (soap->dom->prnt)
      soap->dom = soap->dom->prnt;
    return SOAP_OK;
  }
#endif
#ifndef WITH_LEAN
  if (soap->mode & SOAP_XML_CANONICAL)
    soap_pop_namespace(soap);
  if (soap->mode & SOAP_XML_INDENT)
  { if (!soap->body)
    { if (soap_send_raw(soap, soap_indent, soap->level < sizeof(soap_indent) ? soap->level : sizeof(soap_indent) - 1))
        return soap->error;
    }
    soap->body = 0;
  }
  if ((soap->mode & SOAP_XML_DEFAULTNS) && (s = strchr(tag, ':')))
  { soap_pop_namespace(soap);
    tag = s + 1;
  }
#endif
  if (soap_send_raw(soap, "</", 2)
   || soap_send(soap, tag))
    return soap->error;
  soap->level--;	/* decrement level just before > */
  return soap_send_raw(soap, ">", 1);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_ref(struct soap *soap, const char *tag, int id, int href)
{ const char *s = "ref";
  int n = 1;
  if (soap->version == 1)
  { s = "href";
    n = 0;
  }
  else if (soap->version == 2)
    s = "SOAP-ENC:ref";
  (SOAP_SNPRINTF(soap->href, sizeof(soap->href), sizeof(SOAP_BASEREFNAME) + 21), "#" SOAP_BASEREFNAME "%d", href);
  return soap_element_href(soap, tag, id, s, soap->href + n);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_href(struct soap *soap, const char *tag, int id, const char *ref, const char *val)
{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Element '%s' reference %s='%s'\n", tag, ref, val));
  if (soap_element(soap, tag, id, NULL)
   || soap_attribute(soap, ref, val)
   || soap_element_start_end_out(soap, tag))
    return soap->error;
  soap->body = 0;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_null(struct soap *soap, const char *tag, int id, const char *type)
{ struct soap_attribute *tp = NULL;
  for (tp = soap->attributes; tp; tp = tp->next)
    if (tp->visible)
      break;
  if (tp || (soap->version == 2 && soap->position > 0) || id > 0 || (soap->mode & SOAP_XML_NIL))
  { if (soap_element(soap, tag, id, type)
     || (!tp && soap_attribute(soap, "xsi:nil", "true"))
     || soap_element_start_end_out(soap, tag))
      return soap->error;
    soap->body = 0;
  }
  else
  { soap->null = 1;
    soap->position = 0;
    soap->mustUnderstand = 0;
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_nil(struct soap *soap, const char *tag)
{ if (soap_element(soap, tag, -1, NULL)
   || (soap_attribute(soap, "xsi:nil", "true")))
    return soap->error;
  return soap_element_start_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_id(struct soap *soap, const char *tag, int id, const void *p, const void *a, int n, const char *type, int t, char **mark)
{ (void)a; (void)n;
  if (!p)
  { soap->error = soap_element_null(soap, tag, id, type);
    return -1;
  }
#ifndef WITH_NOIDREF
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Element_id %p type=%d id=%d\n", p, t, id));
  if ((!soap->encodingStyle && !(soap->omode & SOAP_XML_GRAPH)) || (soap->omode & SOAP_XML_TREE))
    return soap_check_and_mark(soap, p, t, mark);
  if (mark)
    *mark = NULL;
  if (id < -1)
    return soap_embed(soap, p, a, n, t);
  else if (id <= 0)
  { struct soap_plist *pp;
    if (a)
      id = soap_array_pointer_lookup(soap, p, a, n, t, &pp);
    else
      id = soap_pointer_lookup(soap, p, t, &pp);
    if (id)
    { if (soap_is_embedded(soap, pp))
      { soap_element_ref(soap, tag, 0, id);
        return -1;
      }
      if (soap_is_single(soap, pp))
        return 0;
      soap_set_embedded(soap, pp);
    }
  }
  return id;
#else
  return soap_check_and_mark(soap, p, t, mark);
#endif
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_check_and_mark(struct soap *soap, const void *p, int t, char **mark)
{ if (mark)
  { struct soap_plist *pp;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Check %p and mark %p\n", p, mark));
    if (!soap_pointer_lookup(soap, p, t, &pp))
      if (!soap_pointer_enter(soap, p, NULL, 0, t, &pp))
	return -1;
    if (soap->mode & SOAP_IO_LENGTH)
    { if (pp->mark1 > 0)
        return -1;
      pp->mark1 = 1;
      *mark = &pp->mark1;
    }
    else
    { if (pp->mark2 > 0)
        return -1;
      pp->mark2 = 1;
      *mark = &pp->mark2;
    }
  }
  return 0;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void *
SOAP_FMAC2
soap_mark_lookup(struct soap *soap, const void *p, int t, struct soap_plist **ppp, char **mark)
{ if (!soap)
    return NULL;
  if (mark || !(soap->mode & SOAP_XML_TREE))
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Mark lookup %p type %d\n", p, t));
    if (!soap_pointer_lookup(soap, p, t, ppp))
    { if (!soap_pointer_enter(soap, p, NULL, 0, t, ppp))
        return NULL;
    }
    else if (!(soap->mode & SOAP_XML_TREE))
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Mark found %p\n", (*ppp)->dup));
      return (*ppp)->dup;
    }
    if (mark)
    { if ((*ppp)->mark1 > 0)
	(*ppp)->mark1 = 2; /* cycle */
      else
        (*ppp)->mark1 = 1; /* cycle detection */
      *mark = &(*ppp)->mark1;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Mark cycle %d\n", (*ppp)->mark1));
    }
  }
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_mark_cycle(struct soap *soap, struct soap_plist *pp)
{ (void)soap;
  return pp && pp->mark1 == 2 && (soap->mode & SOAP_XML_TREE);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_mark_dup(struct soap *soap, void *a, struct soap_plist *pp)
{ (void)soap;
  if (pp)
    pp->dup = a;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_unmark(struct soap *soap, char *mark)
{ (void)soap;
  if (mark)
    *mark = 0; /* release detection */
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_result(struct soap *soap, const char *tag)
{ if (soap->version == 2 && soap->encodingStyle)
  { if (soap_element(soap, "SOAP-RPC:result", 0, NULL)
     || soap_attribute(soap, "xmlns:SOAP-RPC", soap_rpc)
     || soap_element_start_end_out(soap, NULL)
     || soap_string_out(soap, tag, 0)
     || soap_element_end_out(soap, "SOAP-RPC:result"))
      return soap->error;
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_check_result(struct soap *soap, const char *tag)
{ (void)tag;
  if (soap->version == 2 && soap->encodingStyle)
  { soap_instring(soap, ":result", NULL, NULL, 0, 2, -1, -1, NULL);
    /* just ignore content for compliance reasons, but should compare tag to element's QName value? */
  }
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_attribute(struct soap *soap, const char *name, const char *value)
{ DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Attribute '%s'='%s'\n", name, value));
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && !(soap->mode & SOAP_XML_CANONICAL) && soap->dom)
  { struct soap_dom_attribute *a = (struct soap_dom_attribute*)soap_malloc(soap, sizeof(struct soap_dom_attribute));
    if (!a)
      return soap->error;
    a->next = soap->dom->atts;
    a->nstr = NULL;
    a->name = soap_strdup(soap, name);
    a->data = soap_strdup(soap, value);
    a->wide = NULL;
    a->soap = soap;
    soap->dom->atts = a;
    return SOAP_OK;
  }
#endif
#ifndef WITH_LEAN
  if (soap->mode & SOAP_XML_CANONICAL)
  { /* push namespace */
    if (!strncmp(name, "xmlns", 5) && (name[5] == ':' || name[5] == '\0'))
      soap_push_ns(soap, name + 5 + (name[5] == ':'), value, 0);
    else if (soap_set_attr(soap, name, value, 1))
      return soap->error;
  }
  else
#endif
  { if (soap_send_raw(soap, " ", 1)
     || soap_send(soap, name))
      return soap->error;
    if (value)
      if (soap_send_raw(soap, "=\"", 2)
       || soap_string_out(soap, value, 1)
       || soap_send_raw(soap, "\"", 1))
        return soap->error;
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_begin_in(struct soap *soap, const char *tag, int nillable, const char *type)
{ if (!soap_peek_element(soap))
  { if (soap->other)
      return soap->error = SOAP_TAG_MISMATCH;
    if (tag && *tag == '-')
      return SOAP_OK;
    if (!(soap->error = soap_match_tag(soap, soap->tag, tag)))
    { soap->peeked = 0;
      if (type && *soap->type && soap_match_tag(soap, soap->type, type))
        return soap->error = SOAP_TYPE;
      if (!nillable && soap->null && (soap->mode & SOAP_XML_STRICT))
        return soap->error = SOAP_NULL;
      if (soap->body)
        soap->level++;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Begin tag found (level=%u) '%s'='%s'\n", soap->level, soap->tag, tag ? tag : SOAP_STR_EOS ));
      soap->error = SOAP_OK;
    }
  }
  else if (soap->error == SOAP_NO_TAG && tag && *tag == '-')
    soap->error = SOAP_OK;
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_element_end_in(struct soap *soap, const char *tag)
{ soap_wchar c;
  char *s;
  int n = 0;
  if (tag && *tag == '-')
    return SOAP_OK;
  if (soap->error == SOAP_NO_TAG)
    soap->error = SOAP_OK;
#ifdef WITH_DOM
  /* this whitespace or mixed content is significant for DOM */
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { if (!soap->peeked && !soap_string_in(soap, 3, -1, -1, NULL))
      return soap->error;
    if (soap->dom->prnt)
      soap->dom = soap->dom->prnt;
  }
#endif
  if (soap->peeked)
  { if (*soap->tag)
      n++;
    soap->peeked = 0;
  }
  do
  { while (((c = soap_get(soap)) != SOAP_TT))
    { if ((int)c == EOF)
        return soap->error = SOAP_CHK_EOF;
      if (c == SOAP_LT)
        n++;
      else if (c == '/')
      { c = soap_get(soap);
        if (c == SOAP_GT)
          n--;
        else
          soap_unget(soap, c);
      }
    }
  } while (n--);
  s = soap->tag;
  n = sizeof(soap->tag);
  while (soap_notblank(c = soap_get(soap)))
  { if (--n > 0)
      *s++ = (char)c;
  }
  *s = '\0';
  if ((int)c == EOF)
    return soap->error = SOAP_CHK_EOF;
  while (soap_blank(c))
    c = soap_get(soap);
  if (c != SOAP_GT)
    return soap->error = SOAP_SYNTAX_ERROR;
#ifndef WITH_LEAN
#ifdef WITH_DOM
  if (soap->feltendin)
  { soap->level--;
    return soap->error = soap->feltendin(soap, soap->tag, tag);
  }
#endif
  if (tag && (soap->mode & SOAP_XML_STRICT))
  { soap_pop_namespace(soap);
    if (soap_match_tag(soap, soap->tag, tag))
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End tag '%s' does not match '%s'\n", soap->tag, tag ? tag : SOAP_STR_EOS));
      return soap->error = SOAP_SYNTAX_ERROR;
    }
  }
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "End tag found (level=%u) '%s'='%s'\n", soap->level, soap->tag, tag ? tag : SOAP_STR_EOS));
  soap->level--;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char *
SOAP_FMAC2
soap_attr_value(struct soap *soap, const char *name, int flag)
{ struct soap_attribute *tp;
  if (*name == '-')
    return SOAP_STR_EOS;
  for (tp = soap->attributes; tp; tp = tp->next)
  { if (tp->visible && !soap_match_tag(soap, tp->name, name))
      break;
  }
  if (tp)
  { if (flag == 4 || (flag == 2 && (soap->mode & SOAP_XML_STRICT)))
      soap->error = SOAP_PROHIBITED;
    else
      return tp->value;
  }
  else if (flag == 3 || (flag == 1 && (soap->mode & SOAP_XML_STRICT)))
    soap->error = SOAP_REQUIRED;
  else
    soap->error = SOAP_OK;
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_attr(struct soap *soap, const char *name, const char *value, int flag)
{ struct soap_attribute *tp;
  if (*name == '-')
    return SOAP_OK;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Set attribute %s='%s'\n", name, value ? value : SOAP_STR_EOS));
  for (tp = soap->attributes; tp; tp = tp->next)
  { if (!strcmp(tp->name, name))
      break;
  }
  if (!tp)
  { size_t l = strlen(name);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Allocate attribute %s\n", name));
    if (!(tp = (struct soap_attribute*)SOAP_MALLOC(soap, sizeof(struct soap_attribute) + l)))
      return soap->error = SOAP_EOM;
    tp->ns = NULL;
#ifndef WITH_LEAN
    if ((soap->mode & SOAP_XML_CANONICAL))
    { struct soap_attribute **tpp = &soap->attributes;
      const char *s = strchr(name, ':');
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inserting attribute %s for c14n\n", name));
      if (!strncmp(name, "xmlns", 5))
      { for (; *tpp; tpp = &(*tpp)->next)
          if (strncmp((*tpp)->name, "xmlns", 5) || strcmp((*tpp)->name + 5, name + 5) > 0)
            break;
      }
      else if (!s)
      { for (; *tpp; tpp = &(*tpp)->next)
          if (strncmp((*tpp)->name, "xmlns", 5) && ((*tpp)->ns || strcmp((*tpp)->name, name) > 0))
            break;
      }
      else
      { struct soap_nlist *np = soap_lookup_ns(soap, name, s - name);
        if (np)
          tp->ns = np->ns;
        else
        { struct soap_attribute *tq;
          for (tq = soap->attributes; tq; tq = tq->next)
          { if (!strncmp(tq->name, "xmlns:", 6) && !strncmp(tq->name + 6, name, s - name) && !tq->name[6 + s - name])
            { tp->ns = tq->ns;
              break;
            }
          }
        }
        for (; *tpp; tpp = &(*tpp)->next)
        { int k;
          if (strncmp((*tpp)->name, "xmlns", 5) && (*tpp)->ns && tp->ns && ((k = strcmp((*tpp)->ns, tp->ns)) > 0 || (!k && strcmp((*tpp)->name, name) > 0)))
            break;
        }
      }
      tp->next = *tpp;
      *tpp = tp;
    }
    else
#endif
    { tp->next = soap->attributes;
      soap->attributes = tp;
    }
    soap_strcpy((char*)tp->name, l + 1, name);
    tp->value = NULL;
  }
  else if (tp->visible)
  { return SOAP_OK;
  }
  else if (value && tp->value && tp->size <= strlen(value))
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Free attribute value of %s (free %p)\n", name, tp->value));
    SOAP_FREE(soap, tp->value);
    tp->value = NULL;
    tp->ns = NULL;
  }
  if (value)
  { if (!tp->value)
    { tp->size = strlen(value) + 1;
      if (!(tp->value = (char*)SOAP_MALLOC(soap, tp->size)))
        return soap->error = SOAP_EOM;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Allocate attribute value for %s (%p)\n", tp->name, tp->value));
    }
    soap_strcpy(tp->value, tp->size, value);
    if (!strncmp(tp->name, "xmlns:", 6))
      tp->ns = tp->value;
    tp->visible = 2;
    tp->flag = (short)flag;
#ifndef WITH_LEAN
    if (!strcmp(name, "wsu:Id"))
    { soap->event = SOAP_SEC_BEGIN;
      soap_strcpy(soap->id, sizeof(soap->id), value);
    }
    if ((soap->mode & SOAP_XML_CANONICAL))
    { const char *s = strchr(name, ':');
      if (s) /* should also check default namespace when 'type' is not qualified? */
      { struct soap_nlist *np = soap_lookup_ns(soap, name, s - name);
	if (np && np->ns && soap->local_namespaces)
	{ if ((!strcmp(s + 1, "type") && !strcmp(np->ns, soap->local_namespaces[2].ns)) /* xsi:type QName */
	    || ((!strcmp(s + 1, "arrayType") || !strcmp(s + 1, "itemType")) && !strcmp(np->ns, soap->local_namespaces[1].ns))) /* SOAP-ENC:arrayType and SOAP-ENC:itemType QName */
	  soap_utilize_ns(soap, value);
	}
      }
    }
#endif
  }
  else
    tp->visible = 1;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_clr_attr(struct soap *soap)
{ struct soap_attribute *tp;
#ifndef WITH_LEAN
  if ((soap->mode & SOAP_XML_CANONICAL))
  { while (soap->attributes)
    { tp = soap->attributes->next;
      if (soap->attributes->value)
        SOAP_FREE(soap, soap->attributes->value);
      SOAP_FREE(soap, soap->attributes);
      soap->attributes = tp;
    }
  }
  else
#endif
  { for (tp = soap->attributes; tp; tp = tp->next)
      tp->visible = 0;
  }
}
#endif

/******************************************************************************/
#ifndef PALM_2
static int
soap_getattrval(struct soap *soap, char *s, size_t *n, soap_wchar d)
{ char buf[8];
  size_t i;
  size_t k = *n;
  size_t m = 0;
  char *t = buf;
  for (i = 0; i < k; i++)
  { soap_wchar c;
    if (m)
    { *s++ = *t++;
      m--;
      continue;
    }
    if ((soap->mode & SOAP_C_UTFSTRING))
    { if (((c = soap_get(soap)) & 0x80000000) && c >= -0x7FFFFF80 && c < SOAP_AP)
      { t = buf;
        c &= 0x7FFFFFFF;
        if (c < 0x0800)
          *t++ = (char)(0xC0 | ((c >> 6) & 0x1F));
        else
        { if (c < 0x010000)
          *t++ = (char)(0xE0 | ((c >> 12) & 0x0F));
          else
          { if (c < 0x200000)
            *t++ = (char)(0xF0 | ((c >> 18) & 0x07));
            else
            { if (c < 0x04000000)
              *t++ = (char)(0xF8 | ((c >> 24) & 0x03));
              else
              { *t++ = (char)(0xFC | ((c >> 30) & 0x01));
        	*t++ = (char)(0x80 | ((c >> 24) & 0x3F));
              }
              *t++ = (char)(0x80 | ((c >> 18) & 0x3F));
            }
            *t++ = (char)(0x80 | ((c >> 12) & 0x3F));
          }
          *t++ = (char)(0x80 | ((c >> 6) & 0x3F));
        }
        *t++ = (char)(0x80 | (c & 0x3F));
        m = t - buf - 1;
        if (i + m >= k)
        { soap_unget(soap, c | 0x80000000);
          *n = i;
          return soap->error = SOAP_EOM;
        }
        t = buf;
        *s++ = *t++;
        continue;
      }
    }
    else
      c = soap_getutf8(soap);
    switch (c)
    {
    case SOAP_TT:
      *s++ = '<';
      soap_unget(soap, '/');
      break;
    case SOAP_LT:
      *s++ = '<';
      break;
    case SOAP_GT:
      if (d == ' ')
      { soap_unget(soap, c);
        *s = '\0';
        *n = i + 1;
        return SOAP_OK;
      }
      *s++ = '>';
      break;
    case SOAP_QT:
      if (c == d)
      { *s = '\0';
        *n = i + 1;
        return SOAP_OK;
      }
      *s++ = '"';
      break;
    case SOAP_AP:
      if (c == d)
      { *s = '\0';
        *n = i + 1;
        return SOAP_OK;
      }
      *s++ = '\'';
      break;
    case '\t':
    case '\n':
    case '\r':
    case ' ':
    case '/':
      if (d == ' ')
      { soap_unget(soap, c);
        *s = '\0';
        *n = i + 1;
        return SOAP_OK;
      }
    default:
      if ((int)c == EOF)
      { *s = '\0';
        *n = i + 1;
        return soap->error = SOAP_CHK_EOF;
      }
      *s++ = (char)c;
    }
  }
  return soap->error = SOAP_EOM;
}
#endif

/******************************************************************************/
#ifdef WITH_FAST
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_store_lab(struct soap *soap, const char *s, size_t n)
{ soap->labidx = 0;
  return soap_append_lab(soap, s, n);
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_FAST
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_append_lab(struct soap *soap, const char *s, size_t n)
{ if (soap->labidx + n >= soap->lablen)
  { char *t = soap->labbuf;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Enlarging look-aside buffer to append data, size=%lu\n", (unsigned long)soap->lablen));
    if (soap->lablen == 0)
      soap->lablen = SOAP_LABLEN;
    while (soap->labidx + n >= soap->lablen)
      soap->lablen <<= 1;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New look-aside buffer size=%lu\n", (unsigned long)soap->lablen));
    soap->labbuf = (char*)SOAP_MALLOC(soap, soap->lablen);
    if (!soap->labbuf)
    { if (t)
        SOAP_FREE(soap, t);
      return soap->error = SOAP_EOM;
    }
    if (t)
    { soap_memcpy((void*)soap->labbuf, soap->lablen, (const void*)t, soap->labidx);
      SOAP_FREE(soap, t);
    }
  }
  if (s)
  { soap_memcpy((void*)(soap->labbuf + soap->labidx), soap->lablen - soap->labidx, (const void*)s, n);
    soap->labidx += n;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_peek_element(struct soap *soap)
{
#ifdef WITH_DOM
  struct soap_dom_attribute **att = NULL;
  char *lead = NULL;
#endif
  struct soap_attribute *tp, *tq = NULL;
  const char *t;
  char *s;
  soap_wchar c;
  int i;
  if (soap->peeked)
  { if (!*soap->tag)
      return soap->error = SOAP_NO_TAG;
    return SOAP_OK;
  }
  soap->peeked = 1;
  soap->id[0] = '\0';
  soap->href[0] = '\0';
  soap->type[0] = '\0';
  soap->arrayType[0] = '\0';
  soap->arraySize[0] = '\0';
  soap->arrayOffset[0] = '\0';
  soap->other = 0;
  soap->root = -1;
  soap->position = 0;
  soap->null = 0;
  soap->mustUnderstand = 0;
  /* UTF-8 BOM? */
  c = soap_getchar(soap);
  if (c == 0xEF && soap_get0(soap) == 0xBB)
  { c = soap_get1(soap);
    if ((c = soap_get1(soap)) == 0xBF)
      soap->mode &= ~SOAP_ENC_LATIN;
    else
      soap_unget(soap, (0x0F << 12) | (0xBB << 6) | (c & 0x3F)); /* UTF-8 */
  }
  else if ((c == 0xFE && soap_get0(soap) == 0xFF)  /* UTF-16 BE */
        || (c == 0xFF && soap_get0(soap) == 0xFE)) /* UTF-16 LE */
    return soap->error = SOAP_UTF_ERROR;
  else
    soap_unget(soap, c);
  c = soap_get(soap);
#ifdef WITH_DOM
  /* whitespace leading to tag is not insignificant for DOM */
  if (soap_blank(c))
  { soap->labidx = 0;
    do
    { if (soap_append_lab(soap, NULL, 0))
        return soap->error;
      s = soap->labbuf + soap->labidx;
      i = soap->lablen - soap->labidx;
      soap->labidx = soap->lablen;
      while (soap_blank(c) && i--)
      { *s++ = c;
        c = soap_get(soap);
      }
    } while (soap_blank(c));
    *s = '\0';
    lead = soap->labbuf;
  }
#else
  /* skip space */
  while (soap_blank(c))
    c = soap_get(soap);
#endif
  if (c != SOAP_LT)
  { *soap->tag = '\0';
    if ((int)c == EOF)
      return soap->error = SOAP_CHK_EOF;
    soap_unget(soap, c);
#ifdef WITH_DOM
    /* whitespace leading to end tag is significant for DOM */
    if ((soap->mode & SOAP_XML_DOM) && soap->dom)
    { if (lead && *lead)
        soap->dom->tail = soap_strdup(soap, lead);
      else
        soap->dom->tail = (char*)SOAP_STR_EOS;
    }
#endif
    return soap->error = SOAP_NO_TAG;
  }
  do c = soap_get1(soap);
  while (soap_blank(c));
  s = soap->tag;
  i = sizeof(soap->tag);
  while (c != '>' && c != '/' && soap_notblank(c) && (int)c != EOF)
  { if (--i > 0)
      *s++ = (char)c;
    c = soap_get1(soap);
  }
  *s = '\0';
  while (soap_blank(c))
    c = soap_get1(soap);
#ifdef WITH_DOM
  if (soap->mode & SOAP_XML_DOM)
  { struct soap_dom_element *elt;
    elt = (struct soap_dom_element*)soap_malloc(soap, sizeof(struct soap_dom_element));
    if (!elt)
      return soap->error;
    elt->next = NULL;
    elt->nstr = NULL;
    elt->name = soap_strdup(soap, soap->tag);
    elt->prnt = soap->dom;
    elt->elts = NULL;
    elt->atts = NULL;
    elt->data = NULL;
    elt->wide = NULL;
    elt->type = 0;
    elt->node = NULL;
    elt->head = soap_strdup(soap, lead);
    elt->tail = NULL;
    elt->soap = soap;
    if (soap->dom)
    { struct soap_dom_element *p = soap->dom->elts;
      if (p)
      { while (p->next)
          p = p->next;
        p->next = elt;
      }
      else
        soap->dom->elts = elt;
    }
    soap->dom = elt;
    att = &elt->atts;
  }
#endif
  soap_pop_namespace(soap);
  for (tp = soap->attributes; tp; tp = tp->next)
    tp->visible = 0;
  while ((int)c != EOF && c != '>' && c != '/')
  { s = soap->tmpbuf;
    i = sizeof(soap->tmpbuf);
    while (c != '=' && c != '>' && c != '/' && soap_notblank(c) && (int)c != EOF)
    { if (--i > 0)
        *s++ = (char)c;
      c = soap_get1(soap);
    }
    *s = '\0';
    if (i == sizeof(soap->tmpbuf))
      return soap->error = SOAP_SYNTAX_ERROR;
#ifdef WITH_DOM
    /* add attribute name to dom */
    if (att)
    { *att = (struct soap_dom_attribute*)soap_malloc(soap, sizeof(struct soap_dom_attribute));
       if (!*att)
         return soap->error;
       (*att)->next = NULL;
       (*att)->nstr = NULL;
       (*att)->name = soap_strdup(soap, soap->tmpbuf);
       (*att)->data = NULL;
       (*att)->wide = NULL;
       (*att)->soap = soap;
    }
#endif
    if (!strncmp(soap->tmpbuf, "xmlns", 5))
    { if (soap->tmpbuf[5] == ':')
        t = soap->tmpbuf + 6;
      else if (soap->tmpbuf[5])
        t = NULL;
      else
        t = SOAP_STR_EOS;
    }
    else
      t = NULL;
    tq = NULL;
    for (tp = soap->attributes; tp; tq = tp, tp = tp->next)
    { if (!SOAP_STRCMP(tp->name, soap->tmpbuf))
        break;
    }
    if (!tp)
    { size_t l = strlen(soap->tmpbuf);
      tp = (struct soap_attribute*)SOAP_MALLOC(soap, sizeof(struct soap_attribute) + l);
      if (!tp)
        return soap->error = SOAP_EOM;
      soap_strcpy((char*)tp->name, l + 1, soap->tmpbuf);
      tp->value = NULL;
      tp->size = 0;
      tp->ns = NULL;
      /* if attribute name is qualified, append it to the end of the list */
      if (tq && strchr(soap->tmpbuf, ':'))
      { tq->next = tp;
        tp->next = NULL;
      }
      else
      { tp->next = soap->attributes;
        soap->attributes = tp;
      }
    }
    while (soap_blank(c))
      c = soap_get1(soap);
    if (c == '=')
    { size_t k;
      do c = soap_getutf8(soap);
      while (soap_blank(c));
      if (c != SOAP_QT && c != SOAP_AP)
      { soap_unget(soap, c);
        c = ' '; /* blank delimiter */
      }
      k = tp->size;
      if (soap_getattrval(soap, tp->value, &k, c))
      {
#ifdef WITH_FAST
        if (soap->error != SOAP_EOM)
          return soap->error;
        soap->error = SOAP_OK;
        if (soap_store_lab(soap, tp->value, k))
          return soap->error;
        if (tp->value)
          SOAP_FREE(soap, tp->value);
        tp->value = NULL;
        for (;;)
        { k = soap->lablen - soap->labidx;
          if (soap_getattrval(soap, soap->labbuf + soap->labidx, &k, c))
          { if (soap->error != SOAP_EOM)
              return soap->error;
            soap->error = SOAP_OK;
            soap->labidx = soap->lablen;
            if (soap_append_lab(soap, NULL, 0))
              return soap->error;
          }
          else
            break;
        }
        if (soap->labidx)
          tp->size = soap->lablen;
        else
        { tp->size = strlen(soap->labbuf) + 1;
          if (tp->size < SOAP_LABLEN)
            tp->size = SOAP_LABLEN;
        }
        if (!(tp->value = (char*)SOAP_MALLOC(soap, tp->size)))
          return soap->error = SOAP_EOM;
        soap_strcpy(tp->value, tp->size, soap->labbuf);
#else
        tp->size = k;
        if (soap->error != SOAP_EOM)
          return soap->error;
        soap->error = SOAP_OK;
        if (soap_new_block(soap) == NULL)
          return soap->error;
        for (;;)
        { if (!(s = (char*)soap_push_block(soap, NULL, SOAP_BLKLEN)))
            return soap->error;
          k = SOAP_BLKLEN;
          if (soap_getattrval(soap, s, &k, c))
          { if (soap->error != SOAP_EOM)
              return soap->error;
            soap->error = SOAP_OK;
            soap_size_block(soap, NULL, k);
          }
          else
            break;
        }
        k = tp->size + soap->blist->size;
        if (!(s = (char*)SOAP_MALLOC(soap, k)))
          return soap->error = SOAP_EOM;
        if (tp->value)
        { soap_memcpy((void*)s, k, (const void*)tp->value, tp->size);
          SOAP_FREE(soap, tp->value);
        }
        soap_save_block(soap, NULL, s + tp->size, 0);
        tp->value = s;
        tp->size = k;
#endif
      }
      do c = soap_get1(soap);
      while (soap_blank(c));
      tp->visible = 2; /* seen this attribute w/ value */
#ifdef WITH_DOM
      if (att)
        (*att)->data = soap_strdup(soap, tp->value);
#endif
    }
    else
      tp->visible = 1; /* seen this attribute w/o value */
#ifdef WITH_DOM
    if (att)
      att = &(*att)->next;
#endif
    if (t && tp->value)
    { if (soap_push_namespace(soap, t, tp->value) == NULL)
        return soap->error;
    }
  }
#ifdef WITH_DOM
  if (att)
  { soap->dom->nstr = soap_current_namespace(soap, soap->tag);
    for (att = &soap->dom->atts; *att; att = &(*att)->next)
      (*att)->nstr = soap_current_namespace(soap, (*att)->name);
  }
#endif
  if ((int)c == EOF)
    return soap->error = SOAP_CHK_EOF;
  if (!(soap->body = (c != '/')))
    do c = soap_get1(soap);
    while (soap_blank(c));
#ifdef WITH_DOM
  if (soap->mode & SOAP_XML_DOM)
  { if (!soap->body && soap->dom->prnt)
      soap->dom = soap->dom->prnt;
  }
#endif
  for (tp = soap->attributes; tp; tp = tp->next)
  { if (tp->visible && tp->value)
    {
#ifndef WITH_NOIDREF
      if (!strcmp(tp->name, "id"))
      { if ((soap->version > 0 && !(soap->imode & SOAP_XML_TREE))
         || (soap->mode & SOAP_XML_GRAPH))
        { *soap->id = '#';
          soap_strcpy(soap->id + 1, sizeof(soap->id) - 1, tp->value);
        }
      }
      else if (!strcmp(tp->name, "href"))
      { if ((soap->version == 1 && !(soap->imode & SOAP_XML_TREE))
         || (soap->mode & SOAP_XML_GRAPH)
	 || ((soap->mode & (SOAP_ENC_MTOM | SOAP_ENC_DIME)) && *tp->value != '#'))
          soap_strcpy(soap->href, sizeof(soap->href), tp->value);
      }
      else if (!strcmp(tp->name, "ref"))
      { if ((soap->version == 2 && !(soap->imode & SOAP_XML_TREE))
         || (soap->mode & SOAP_XML_GRAPH))
        { *soap->href = '#';
          soap_strcpy(soap->href + (*tp->value != '#'), sizeof(soap->href) - 1, tp->value);
        }
      }
#else
      if (!strcmp(tp->name, "href"))
      { if ((soap->mode & (SOAP_ENC_MTOM | SOAP_ENC_DIME)) && *tp->value != '#')
          soap_strcpy(soap->href, sizeof(soap->href), tp->value);
      }
#endif
      else if (!soap_match_tag(soap, tp->name, "xsi:type"))
      { soap_strcpy(soap->type, sizeof(soap->type), tp->value);
      }
      else if ((!soap_match_tag(soap, tp->name, "xsi:null")
             || !soap_match_tag(soap, tp->name, "xsi:nil"))
            && (!strcmp(tp->value, "1")
             || !strcmp(tp->value, "true")))
      { soap->null = 1;
      }
      else if (!soap_match_tag(soap, tp->name, "SOAP-ENV:encodingStyle"))
      { if (!soap->encodingStyle)
	  soap->encodingStyle = SOAP_STR_EOS;
	soap_get_version(soap);
      }
      else if (soap->version == 1)
      {
        if (!soap_match_tag(soap, tp->name, "SOAP-ENC:arrayType"))
        { s = soap_strrchr(tp->value, '[');
          if (s && (size_t)(s - tp->value) < sizeof(soap->arrayType))
          { soap_strncpy(soap->arrayType, sizeof(soap->arrayType), tp->value, s - tp->value);
            soap_strcpy(soap->arraySize, sizeof(soap->arraySize), s);
          }
          else
            soap_strcpy(soap->arrayType, sizeof(soap->arrayType), tp->value);
        }
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENC:offset"))
          soap_strcpy(soap->arrayOffset, sizeof(soap->arrayOffset), tp->value);
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENC:position"))
          soap->position = soap_getposition(tp->value, soap->positions);
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENC:root"))
          soap->root = ((!strcmp(tp->value, "1") || !strcmp(tp->value, "true")));
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENV:mustUnderstand")
              && (!strcmp(tp->value, "1") || !strcmp(tp->value, "true")))
          soap->mustUnderstand = 1;
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENV:actor"))
        { if ((!soap->actor || strcmp(soap->actor, tp->value))
           && strcmp(tp->value, "http://schemas.xmlsoap.org/soap/actor/next"))
            soap->other = 1;
        }
      }
      else if (soap->version == 2)
      {
#ifndef WITH_NOIDREF
        if (!soap_match_tag(soap, tp->name, "SOAP-ENC:id"))
        { *soap->id = '#';
          soap_strcpy(soap->id + 1, sizeof(soap->id) - 1, tp->value);
        }
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENC:ref"))
        { *soap->href = '#';
          soap_strcpy(soap->href + (*tp->value != '#'), sizeof(soap->href) - 1, tp->value);
        }
        else
#endif
        if (!soap_match_tag(soap, tp->name, "SOAP-ENC:itemType"))
          soap_strcpy(soap->arrayType, sizeof(soap->arrayType), tp->value);
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENC:arraySize"))
          soap_strcpy(soap->arraySize, sizeof(soap->arraySize), tp->value);
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENV:mustUnderstand")
              && (!strcmp(tp->value, "1") || !strcmp(tp->value, "true")))
          soap->mustUnderstand = 1;
        else if (!soap_match_tag(soap, tp->name, "SOAP-ENV:role"))
        { if ((!soap->actor || strcmp(soap->actor, tp->value))
           && strcmp(tp->value, "http://www.w3.org/2003/05/soap-envelope/role/next"))
            soap->other = 1;
        }
      }
      else
      { if (!soap_match_tag(soap, tp->name, "wsdl:required") && !strcmp(tp->value, "true"))
          soap->mustUnderstand = 1;
      }
    }
  }
#ifdef WITH_DOM
  if (soap->feltbegin)
    return soap->error = soap->feltbegin(soap, soap->tag);
#endif
  return soap->error = SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_retry(struct soap *soap)
{ soap->error = SOAP_OK;
  soap_revert(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void
SOAP_FMAC2
soap_revert(struct soap *soap)
{ if (!soap->peeked)
  { soap->peeked = 1;
    if (soap->body)
      soap->level--;
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Reverting to last element '%s' (level=%u)\n", soap->tag, soap->level));
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_string_out(struct soap *soap, const char *s, int flag)
{ const char *t;
  soap_wchar c;
  soap_wchar mask = (soap_wchar)0xFFFFFF80UL;
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { soap->dom->data = soap_strdup(soap, s);
    return SOAP_OK;
  }
#endif
  if (flag == 2 || soap->mode & SOAP_C_UTFSTRING)
    mask = 0;
  t = s;
  while ((c = *t++))
  { switch (c)
    {
    case 0x09:
      if (flag)
      { if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&#x9;", 5))
          return soap->error;
        s = t;
      }
      break;
    case 0x0A:
      if (flag || !(soap->mode & SOAP_XML_CANONICAL))
      { if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&#xA;", 5))
          return soap->error;
        s = t;
      }
      break;
    case '&':
      if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&amp;", 5))
        return soap->error;
      s = t;
      break;
    case '<':
      if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&lt;", 4))
        return soap->error;
      s = t;
      break;
    case '>':
      if (!flag)
      { if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&gt;", 4))
          return soap->error;
        s = t;
      }
      break;
    case '"':
      if (flag)
      { if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&quot;", 6))
          return soap->error;
        s = t;
      }
      break;
    case 0x7F:
      if (soap_send_raw(soap, s, t - s - 1) || soap_send_raw(soap, "&#x7F;", 6))
        return soap->error;
      s = t;
      break;
    default:
#ifndef WITH_LEANER
#ifdef HAVE_MBTOWC
      if (soap->mode & SOAP_C_MBSTRING)
      { wchar_t wc;
        int m = mbtowc(&wc, t - 1, MB_CUR_MAX);
        if (m > 0 && !((soap_wchar)wc == c && m == 1 && c < 0x80))
        { if (soap_send_raw(soap, s, t - s - 1) || soap_pututf8(soap, (unsigned long)wc))
            return soap->error;
          s = t += m - 1;
          continue;
        }
      }
#endif
#endif
#ifndef WITH_NOSTRINGTOUTF8
      if ((c & mask) || !(c & 0xFFFFFFE0UL))
      { if (soap_send_raw(soap, s, t - s - 1) || soap_pututf8(soap, (unsigned char)c))
          return soap->error;
        s = t;
      }
#endif
    }
  }
  return soap_send_raw(soap, s, t - s - 1);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
char *
SOAP_FMAC2
soap_string_in(struct soap *soap, int flag, long minlen, long maxlen, const char *pattern)
{ char *s;
  char *t = NULL;
  size_t i;
  long l = 0;
  int n = 0, f = 0, m = 0;
  soap_wchar c;
#if !defined(WITH_LEANER) && defined(HAVE_WCTOMB)
  char buf[MB_LEN_MAX > 8 ? MB_LEN_MAX : 8];
#else
  char buf[8];
#endif
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Reading string content, flag=%d\n", flag));
  if (soap->peeked && *soap->tag)
  {
#ifndef WITH_LEAN
    struct soap_attribute *tp;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String content includes tag '%s' and attributes\n", soap->tag));
    t = soap->tmpbuf;
    *t = '<';
    soap_strcpy(t + 1, sizeof(soap->tmpbuf) - 1, soap->tag);
    t += strlen(t);
    for (tp = soap->attributes; tp; tp = tp->next)
    { if (tp->visible)
      { size_t l = strlen(tp->name);
	if (t + l + 1 >= soap->tmpbuf + sizeof(soap->tmpbuf))
	  break; /* too many or attribute values to large */
        *t++ = ' ';
        soap_strncpy(t, sizeof(soap->tmpbuf) - (t - soap->tmpbuf), tp->name, l);
        t += l;
        if (tp->value)
        { l = strlen(tp->value);
          if (t + l + 3 >= soap->tmpbuf + sizeof(soap->tmpbuf))
            break; /* too many or attribute values to large */
	  *t++ = '=';
          *t++ = '"';
          soap_strncpy(t, sizeof(soap->tmpbuf) - (t - soap->tmpbuf), tp->value, l);
          t += l;
          *t++ = '"';
        }
      }
    }
    if (!soap->body)
      *t++ = '/';
    *t++ = '>';
    *t = '\0';
    t = soap->tmpbuf;
    m = (int)strlen(soap->tmpbuf);
#endif
    if (soap->body)
      n = 1;
    f = 1;
    soap->peeked = 0;
  }
#ifdef WITH_CDATA
  if (!flag)
  { int state = 0;
#ifdef WITH_FAST
    soap->labidx = 0;			/* use look-aside buffer */
#else
    if (soap_new_block(soap) == NULL)
      return NULL;
#endif
    for (;;)
    {
#ifdef WITH_FAST
      size_t k;
      if (soap_append_lab(soap, NULL, 0))	/* allocate more space in look-aside buffer if necessary */
        return NULL;
      s = soap->labbuf + soap->labidx;	/* space to populate */
      k = soap->lablen - soap->labidx;	/* number of bytes available */
      soap->labidx = soap->lablen;	/* claim this space */
#else
      size_t k = SOAP_BLKLEN;
      if (!(s = (char*)soap_push_block(soap, NULL, k)))
        return NULL;
#endif
      for (i = 0; i < k; i++)
      { if (m > 0)
        { *s++ = *t++;	/* copy multibyte characters */
          m--;
          continue;
        }
        c = soap_getchar(soap);
        if ((int)c == EOF)
          goto end;
        if ((c >= 0x80 || c < SOAP_AP) && state != 1 && !(soap->mode & SOAP_ENC_LATIN))
        { if ((c & 0x7FFFFFFF) >= 0x80)
          { soap_unget(soap, c);
            c = soap_getutf8(soap);
          }
          if ((c & 0x7FFFFFFF) >= 0x80 && (!flag || (soap->mode & SOAP_C_UTFSTRING)))
          { c &= 0x7FFFFFFF;
            t = buf;
            if (c < 0x0800)
              *t++ = (char)(0xC0 | ((c >> 6) & 0x1F));
            else
            { if (c < 0x010000)
                *t++ = (char)(0xE0 | ((c >> 12) & 0x0F));
              else
              { if (c < 0x200000)
                  *t++ = (char)(0xF0 | ((c >> 18) & 0x07));
                else
                { if (c < 0x04000000)
                    *t++ = (char)(0xF8 | ((c >> 24) & 0x03));
                  else
                  { *t++ = (char)(0xFC | ((c >> 30) & 0x01));
                    *t++ = (char)(0x80 | ((c >> 24) & 0x3F));
                  }
                  *t++ = (char)(0x80 | ((c >> 18) & 0x3F));
                }
                *t++ = (char)(0x80 | ((c >> 12) & 0x3F));
              }
              *t++ = (char)(0x80 | ((c >> 6) & 0x3F));
            }
            *t++ = (char)(0x80 | (c & 0x3F));
            m = (int)(t - buf) - 1;
            t = buf;
            *s++ = *t++;
            continue;
          }
        }
        switch (state)
        { case 1:
            if (c == ']')
              state = 4;
            *s++ = (char)c;
            continue;
          case 2:
            if (c == '-')
              state = 6;
            *s++ = (char)c;
            continue;
          case 3:
            if (c == '?')
              state = 8;
            *s++ = (char)c;
            continue;
          /* CDATA */
          case 4:
            if (c == ']')
              state = 5;
            else
              state = 1;
            *s++ = (char)c;
            continue;
          case 5:
            if (c == '>')
              state = 0;
            else if (c != ']')
              state = 1;
            *s++ = (char)c;
            continue;
          /* comment */
          case 6:
            if (c == '-')
              state = 7;
            else
              state = 2;
            *s++ = (char)c;
            continue;
          case 7:
            if (c == '>')
              state = 0;
            else if (c != '-')
              state = 2;
            *s++ = (char)c;
            continue;
          /* PI */
          case 8:
            if (c == '>')
              state = 0;
            else if (c != '?')
              state = 3;
            *s++ = (char)c;
            continue;
        }
        switch (c)
        {
        case SOAP_TT:
          if (n == 0)
            goto end;
          n--;
          *s++ = '<';
          t = (char*)"/";
          m = 1;
          break;
        case SOAP_LT:
          if (f && n == 0)
            goto end;
          n++;
          *s++ = '<';
          break;
        case SOAP_GT:
          *s++ = '>';
          break;
        case SOAP_QT:
          *s++ = '"';
          break;
        case SOAP_AP:
          *s++ = '\'';
          break;
        case '/':
          if (n > 0)
          { c = soap_getchar(soap);
            if (c == '>')
              n--;
            soap_unget(soap, c);
          }
          *s++ = '/';
          break;
        case '<':
          c = soap_getchar(soap);
          if (c == '/')
          { if (n == 0)
            { c = SOAP_TT;
              goto end;
            }
            n--;
          }
          else if (c == '!')
          { c = soap_getchar(soap);
            if (c == '[')
            { do c = soap_getchar(soap);
              while ((int)c != EOF && c != '[');
              if ((int)c == EOF)
                 goto end;
              t = (char*)"![CDATA[";
              m = 8;
              state = 1;
            }
            else if (c == '-')
            { if ((c = soap_getchar(soap)) == '-')
                state = 2;
              t = (char*)"!-";
              m = 2;
              soap_unget(soap, c);
            }
            else
            { t = (char*)"!";
              m = 1;
              soap_unget(soap, c);
            }
            *s++ = '<';
            break;
          }
          else if (c == '?')
            state = 3;
          else if (f && n == 0)
          { soap_revget1(soap);
            c = '<';
            goto end;
          }
          else
            n++;
          soap_unget(soap, c);
          *s++ = '<';
          break;
        case '>':
          *s++ = '>';
          break;
        case '"':
          *s++ = '"';
          break;
        default:
#ifndef WITH_LEANER
#ifdef HAVE_WCTOMB
          if (soap->mode & SOAP_C_MBSTRING)
          {
#ifdef WIN32
	    m = 0;
	    wctomb_s(&m, buf, sizeof(buf), (wchar_t)(c & 0x7FFFFFFF));
#else
	    m = wctomb(buf, (wchar_t)(c & 0x7FFFFFFF));
#endif
            if (m >= 1 && m <= (int)MB_CUR_MAX)
            { t = buf;
              *s++ = *t++;
              m--;
            }
            else
            { *s++ = SOAP_UNKNOWN_CHAR;
              m = 0;
            }
          }
          else
#endif
#endif
            *s++ = (char)(c & 0xFF);
        }
        l++;
        if (maxlen >= 0 && l > maxlen)
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String too long: maxlen=%ld\n", maxlen));
          soap->error = SOAP_LENGTH;
          return NULL;
        }
      }
    }
  }
#endif
#ifdef WITH_FAST
  soap->labidx = 0;			/* use look-aside buffer */
#else
  if (soap_new_block(soap) == NULL)
    return NULL;
#endif
  for (;;)
  {
#ifdef WITH_FAST
    size_t k;
    if (soap_append_lab(soap, NULL, 0))	/* allocate more space in look-aside buffer if necessary */
      return NULL;
    s = soap->labbuf + soap->labidx;	/* space to populate */
    k = soap->lablen - soap->labidx;	/* number of bytes available */
    soap->labidx = soap->lablen;	/* claim this space */
#else
    size_t k = SOAP_BLKLEN;
    if (!(s = (char*)soap_push_block(soap, NULL, k)))
      return NULL;
#endif
    for (i = 0; i < k; i++)
    { if (m > 0)
      { *s++ = *t++;	/* copy multibyte characters */
        m--;
        continue;
      }
#ifndef WITH_CDATA
      if (!flag)
        c = soap_getchar(soap);
      else
#endif
      { c = soap_getutf8(soap);
        if ((soap->mode & SOAP_C_UTFSTRING))
        { if (c >= 0x80 || (c < SOAP_AP && c >= -0x7FFFFF80))
          { c &= 0x7FFFFFFF;
            t = buf;
            if (c < 0x0800)
              *t++ = (char)(0xC0 | ((c >> 6) & 0x1F));
            else
            { if (c < 0x010000)
              *t++ = (char)(0xE0 | ((c >> 12) & 0x0F));
              else
              { if (c < 0x200000)
        	*t++ = (char)(0xF0 | ((c >> 18) & 0x07));
        	else
        	{ if (c < 0x04000000)
        	  *t++ = (char)(0xF8 | ((c >> 24) & 0x03));
        	  else
        	  { *t++ = (char)(0xFC | ((c >> 30) & 0x01));
        	    *t++ = (char)(0x80 | ((c >> 24) & 0x3F));
        	  }
        	  *t++ = (char)(0x80 | ((c >> 18) & 0x3F));
        	}
        	*t++ = (char)(0x80 | ((c >> 12) & 0x3F));
              }
              *t++ = (char)(0x80 | ((c >> 6) & 0x3F));
            }
            *t++ = (char)(0x80 | (c & 0x3F));
            m = (int)(t - buf) - 1;
            t = buf;
            *s++ = *t++;
            l++;
            if (maxlen >= 0 && l > maxlen)
            { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String too long: maxlen=%ld\n", maxlen));
              soap->error = SOAP_LENGTH;
              return NULL;
            }
            continue;
          }
        }
      }
      switch (c)
      {
      case SOAP_TT:
        if (n == 0)
          goto end;
        n--;
        *s++ = '<';
        t = (char*)"/";
        m = 1;
        break;
      case SOAP_LT:
        if (f && n == 0)
          goto end;
        n++;
        *s++ = '<';
        break;
      case SOAP_GT:
        *s++ = '>';
        break;
      case SOAP_QT:
        *s++ = '"';
        break;
      case SOAP_AP:
        *s++ = '\'';
        break;
      case '/':
        if (n > 0)
        { if (!flag)
          { c = soap_getchar(soap);
            if (c == '>')
              n--;
          }
          else
          { c = soap_get(soap);
            if (c == SOAP_GT)
              n--;
          }
          soap_unget(soap, c);
        }
        *s++ = '/';
        break;
      case (soap_wchar)('<' | 0x80000000):
        if (flag)
          *s++ = '<';
        else
        { *s++ = '&';
          t = (char*)"lt;";
          m = 3;
        }
        break;
      case (soap_wchar)('>' | 0x80000000):
        if (flag)
          *s++ = '>';
        else
        { *s++ = '&';
          t = (char*)"gt;";
          m = 3;
        }
        break;
      case (soap_wchar)('&' | 0x80000000):
        if (flag)
          *s++ = '&';
        else
        { *s++ = '&';
          t = (char*)"amp;";
          m = 4;
        }
        break;
      case (soap_wchar)('"' | 0x80000000):
        if (flag)
          *s++ = '"';
        else
        { *s++ = '&';
          t = (char*)"quot;";
          m = 5;
        }
        break;
      case (soap_wchar)('\'' | 0x80000000):
        if (flag)
          *s++ = '\'';
        else
        { *s++ = '&';
          t = (char*)"apos;";
          m = 5;
        }
        break;
      default:
        if ((int)c == EOF)
          goto end;
#ifndef WITH_CDATA
        if (c == '<' && !flag)
        { if (f && n == 0)
            goto end;
          c = soap_getchar(soap);
          soap_unget(soap, c);
          if (c == '/')
          { c = SOAP_TT;
            if (n == 0)
              goto end;
            n--;
          }
          else
            n++;
          *s++ = '<';
          break;
        }
        else
#endif
#ifndef WITH_LEANER
#ifdef HAVE_WCTOMB
        if (soap->mode & SOAP_C_MBSTRING)
        {
#ifdef WIN32
	  m = 0;
	  wctomb_s(&m, buf, sizeof(buf), (wchar_t)(c & 0x7FFFFFFF));
#else
	  m = wctomb(buf, (wchar_t)(c & 0x7FFFFFFF));
#endif
          if (m >= 1 && m <= (int)MB_CUR_MAX)
          { t = buf;
            *s++ = *t++;
            m--;
          }
          else
          { *s++ = SOAP_UNKNOWN_CHAR;
            m = 0;
          }
        }
        else
#endif
#endif
          *s++ = (char)(c & 0xFF);
      }
      l++;
      if (maxlen >= 0 && l > maxlen)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String too long: maxlen=%ld\n", maxlen));
        soap->error = SOAP_LENGTH;
        return NULL;
      }
    }
  }
end:
  soap_unget(soap, c);
  *s = '\0';
#ifdef WITH_FAST
  t = soap_strdup(soap, soap->labbuf);
#else
  soap_size_block(soap, NULL, i + 1);
  t = soap_save_block(soap, NULL, NULL, 0);
#endif
  if (l < minlen)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String too short: %ld chars, minlen=%ld\n", l, minlen));
    soap->error = SOAP_LENGTH;
    return NULL;
  }
#ifndef WITH_LEANER
  if (pattern && soap->fsvalidate && (soap->error = soap->fsvalidate(soap, pattern, s)))
    return NULL;
#else
  (void)pattern;
#endif
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { if (flag == 3)
      soap->dom->tail = t;
    else
      soap->dom->data = t;
  }
#endif
  if (flag == 2)
    if (soap_s2QName(soap, t, &t, minlen, maxlen))
      return NULL;
  return t;
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_wstring_out(struct soap *soap, const wchar_t *s, int flag)
{ const char *t;
  char tmp;
  soap_wchar c;
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
  { wchar_t *r = (wchar_t*)s;
    int n = 1;
    while (*r++)
      n++;
    soap->dom->wide = r = (wchar_t*)soap_malloc(soap, n * sizeof(wchar_t));
    while (n--)
      *r++ = *s++;
    return SOAP_OK;
  }
#endif
  while ((c = *s++))
  { switch (c)
    {
    case 0x09:
      if (flag)
        t = "&#x9;";
      else
        t = "\t";
      break;
    case 0x0A:
      if (flag || !(soap->mode & SOAP_XML_CANONICAL))
        t = "&#xA;";
      else
        t = "\n";
      break;
    case 0x0D:
      t = "&#xD;";
      break;
    case '&':
      t = "&amp;";
      break;
    case '<':
      t = "&lt;";
      break;
    case '>':
      if (flag)
        t = ">";
      else
        t = "&gt;";
      break;
    case '"':
      if (flag)
        t = "&quot;";
      else
        t = "\"";
      break;
    default:
      if (c >= 0x20 && c < 0x80)
      { tmp = (char)c;
        if (soap_send_raw(soap, &tmp, 1))
          return soap->error;
      }
      else
      { /* check for UTF16 encoding when wchar_t is too small to hold UCS */
        if (sizeof(wchar_t) < 4 && (c & 0xFC00) == 0xD800)
        { soap_wchar d = *s++;
          if ((d & 0xFC00) == 0xDC00)
            c = ((c - 0xD800) << 10) + (d - 0xDC00) + 0x10000;
          else
            c = 0xFFFD; /* Malformed */
        }
        if (soap_pututf8(soap, (unsigned long)c))
          return soap->error;
      }
      continue;
    }
    if (soap_send(soap, t))
      return soap->error;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_2
SOAP_FMAC1
wchar_t *
SOAP_FMAC2
soap_wstring_in(struct soap *soap, int flag, long minlen, long maxlen, const char *pattern)
{ wchar_t *s;
  int i, n = 0, f = 0;
  long l = 0;
  soap_wchar c;
  char *t = NULL;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Reading wide string content\n"));
  if (soap->peeked)
  { if (*soap->tag)
    {
#ifndef WITH_LEAN
      struct soap_attribute *tp;
      t = soap->tmpbuf;
      *t = '<';
      soap_strcpy(t + 1, sizeof(soap->tmpbuf) - 1, soap->tag);
      t += strlen(t);
      for (tp = soap->attributes; tp; tp = tp->next)
      { if (tp->visible)
        { size_t l = strlen(tp->name);
	  if (t + l + 1 >= soap->tmpbuf + sizeof(soap->tmpbuf))
            break;
          *t++ = ' ';
          soap_strncpy(t, sizeof(soap->tmpbuf) - (t - soap->tmpbuf), tp->name, l);
          t += l;
          if (tp->value)
	  { l = strlen(tp->value);
	    if (t + l + 3 >= soap->tmpbuf + sizeof(soap->tmpbuf))
	      break;
            *t++ = '=';
            *t++ = '"';
            soap_strncpy(t, sizeof(soap->tmpbuf) - (t - soap->tmpbuf), tp->value, l);
            t += l;
            *t++ = '"';
          }
        }
      }
      if (!soap->body)
        *t++ = '/';
      *t++ = '>';
      *t = '\0';
      t = soap->tmpbuf;
#endif
      if (soap->body)
        n = 1;
      f = 1;
      soap->peeked = 0;
    }
  }
  if (soap_new_block(soap) == NULL)
    return NULL;
  for (;;)
  { if (!(s = (wchar_t*)soap_push_block(soap, NULL, sizeof(wchar_t)*SOAP_BLKLEN)))
      return NULL;
    for (i = 0; i < SOAP_BLKLEN; i++)
    { if (t)
      { *s++ = (wchar_t)*t++;
        if (!*t)
          t = NULL;
        continue;
      }
      c = soap_getutf8(soap);
      switch (c)
      {
      case SOAP_TT:
        if (n == 0)
          goto end;
        n--;
        *s++ = '<';
        soap_unget(soap, '/');
        break;
      case SOAP_LT:
        if (f && n == 0)
          goto end;
        n++;
        *s++ = '<';
        break;
      case SOAP_GT:
        *s++ = '>';
        break;
      case SOAP_QT:
        *s++ = '"';
        break;
      case SOAP_AP:
        *s++ = '\'';
        break;
      case '/':
        if (n > 0)
        { c = soap_getutf8(soap);
          if (c == SOAP_GT)
            n--;
          soap_unget(soap, c);
        }
        *s++ = '/';
        break;
      case '<':
        if (flag)
          *s++ = (soap_wchar)'<';
        else
        { *s++ = (soap_wchar)'&';
          t = (char*)"lt;";
        }
        break;
      case '>':
        if (flag)
          *s++ = (soap_wchar)'>';
        else
        { *s++ = (soap_wchar)'&';
          t = (char*)"gt;";
        }
        break;
      case '"':
        if (flag)
          *s++ = (soap_wchar)'"';
        else
        { *s++ = (soap_wchar)'&';
          t = (char*)"quot;";
        }
        break;
      default:
        if ((int)c == EOF)
          goto end;
        /* use UTF16 encoding when wchar_t is too small to hold UCS */
        if (sizeof(wchar_t) < 4 && c > 0xFFFF)
        { soap_wchar c1, c2;
          c1 = 0xD800 - (0x10000 >> 10) + (c >> 10);
          c2 = 0xDC00 + (c & 0x3FF);
          c = c1;
          soap_unget(soap, c2);
        }
        *s++ = (wchar_t)c & 0x7FFFFFFF;
      }
      l++;
      if (maxlen >= 0 && l > maxlen)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String too long: maxlen=%ld\n", maxlen));
        soap->error = SOAP_LENGTH;
        return NULL;
      }
    }
  }
end:
  soap_unget(soap, c);
  *s = '\0';
  soap_size_block(soap, NULL, sizeof(wchar_t) * (i + 1));
  if (l < minlen)
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "String too short: %ld chars, minlen=%ld\n", l, minlen));
    soap->error = SOAP_LENGTH;
    return NULL;
  }
  s = (wchar_t*)soap_save_block(soap, NULL, NULL, 0);
#ifndef WITH_LEANER
  if (pattern && soap->fwvalidate && (soap->error = soap->fwvalidate(soap, pattern, s)))
    return NULL;
#endif
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
    soap->dom->wide = s;
#endif
  return s;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_int2s(struct soap *soap, int n)
{ return soap_long2s(soap, (long)n);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outint(struct soap *soap, const char *tag, int id, const int *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_long2s(soap, (long)*p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2int(struct soap *soap, const char *s, int *p)
{ if (s)
  { long n;
    char *r;
#ifndef WITH_NOIO
#ifndef WITH_LEAN
    soap_reset_errno;
#endif
#endif
    n = soap_strtol(s, &r, 10);
    if (s == r || *r
#ifndef WITH_LEAN
     || n != (int)n
#endif
#ifndef WITH_NOIO
#ifndef WITH_LEAN
     || soap_errno == SOAP_ERANGE
#endif
#endif
    )
      soap->error = SOAP_TYPE;
    *p = (int)n;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int *
SOAP_FMAC2
soap_inint(struct soap *soap, const char *tag, int *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":int")
   && soap_match_tag(soap, soap->type, ":short")
   && soap_match_tag(soap, soap->type, ":byte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (int*)soap_id_enter(soap, soap->id, p, t, sizeof(int), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (int*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(int), 0, NULL, NULL);
  else if (p)
  { if (soap_s2int(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_long2s(struct soap *soap, long n)
{ (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 20), "%ld", n);
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outlong(struct soap *soap, const char *tag, int id, const long *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_long2s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2long(struct soap *soap, const char *s, long *p)
{ if (s)
  { char *r;
#ifndef WITH_NOIO
#ifndef WITH_LEAN
    soap_reset_errno;
#endif
#endif
    *p = soap_strtol(s, &r, 10);
    if (s == r || *r
#ifndef WITH_NOIO
#ifndef WITH_LEAN
     || soap_errno == SOAP_ERANGE
#endif
#endif
    )
      soap->error = SOAP_TYPE;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
long *
SOAP_FMAC2
soap_inlong(struct soap *soap, const char *tag, long *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":int")
   && soap_match_tag(soap, soap->type, ":short")
   && soap_match_tag(soap, soap->type, ":byte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (long*)soap_id_enter(soap, soap->id, p, t, sizeof(long), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (long*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(long), 0, NULL, NULL);
  else if (p)
  { if (soap_s2long(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_LONG642s(struct soap *soap, LONG64 n)
{ (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 20), SOAP_LONG_FORMAT, n);
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
int
SOAP_FMAC2
soap_outLONG64(struct soap *soap, const char *tag, int id, const LONG64 *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_LONG642s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2LONG64(struct soap *soap, const char *s, LONG64 *p)
{ if (s)
  { char *r;
#ifndef WITH_NOIO
#ifndef WITH_LEAN
    soap_reset_errno;
#endif
#endif
#ifdef HAVE_STRTOLL
    *p = soap_strtoll(s, &r, 10);
#else
    *p = (LONG64)soap_strtol(s, &r, 10);
#endif
    if (s == r || *r
#ifndef WITH_NOIO
#ifndef WITH_LEAN
       || soap_errno == SOAP_ERANGE
#endif
#endif
      )
      soap->error = SOAP_TYPE;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
LONG64 *
SOAP_FMAC2
soap_inLONG64(struct soap *soap, const char *tag, LONG64 *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":integer")
   && soap_match_tag(soap, soap->type, ":positiveInteger")
   && soap_match_tag(soap, soap->type, ":negativeInteger")
   && soap_match_tag(soap, soap->type, ":nonPositiveInteger")
   && soap_match_tag(soap, soap->type, ":nonNegativeInteger")
   && soap_match_tag(soap, soap->type, ":long")
   && soap_match_tag(soap, soap->type, ":int")
   && soap_match_tag(soap, soap->type, ":short")
   && soap_match_tag(soap, soap->type, ":byte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#endif
  p = (LONG64*)soap_id_enter(soap, soap->id, p, t, sizeof(LONG64), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (LONG64*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(LONG64), 0, NULL, NULL);
  else if (p)
  { if (soap_s2LONG64(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_byte2s(struct soap *soap, char n)
{ return soap_long2s(soap, (long)n);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outbyte(struct soap *soap, const char *tag, int id, const char *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_long2s(soap, (long)*p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2byte(struct soap *soap, const char *s, char *p)
{ if (s)
  { long n;
    char *r;
    n = soap_strtol(s, &r, 10);
    if (s == r || *r || n < -128 || n > 127)
      soap->error = SOAP_TYPE;
    *p = (char)n;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
char *
SOAP_FMAC2
soap_inbyte(struct soap *soap, const char *tag, char *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":byte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (char*)soap_id_enter(soap, soap->id, p, t, sizeof(char), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (char*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(char), 0, NULL, NULL);
  else if (p)
  { if (soap_s2byte(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_short2s(struct soap *soap, short n)
{ return soap_long2s(soap, (long)n);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outshort(struct soap *soap, const char *tag, int id, const short *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_long2s(soap, (long)*p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2short(struct soap *soap, const char *s, short *p)
{ if (s)
  { long n;
    char *r;
    n = soap_strtol(s, &r, 10);
    if (s == r || *r || n < -32768 || n > 32767)
      soap->error = SOAP_TYPE;
    *p = (short)n;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
short *
SOAP_FMAC2
soap_inshort(struct soap *soap, const char *tag, short *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":short")
   && soap_match_tag(soap, soap->type, ":byte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (short*)soap_id_enter(soap, soap->id, p, t, sizeof(short), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (short*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(short), 0, NULL, NULL);
  else if (p)
  { if (soap_s2short(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_float2s(struct soap *soap, float n)
{
#if !defined(WITH_C_LOCALE) || !defined(HAVE_SPRINTF_L)
  char *s;
#endif
  if (soap_isnan((double)n))
    return "NaN";
  if (soap_ispinff(n))
    return "INF";
  if (soap_isninff(n))
    return "-INF";
#if defined(WITH_C_LOCALE) && defined(HAVE_SPRINTF_L)
# ifdef WIN32
  _sprintf_s_l(soap->tmpbuf, _countof(soap->tmpbuf), soap->float_format, SOAP_LOCALE(soap), n);
# else
  sprintf_l(soap->tmpbuf, SOAP_LOCALE(soap), soap->float_format, n);
# endif
#else
  (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 20), soap->float_format, n);
  s = strchr(soap->tmpbuf, ',');	/* convert decimal comma to DP */
  if (s)
    *s = '.';
#endif
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outfloat(struct soap *soap, const char *tag, int id, const float *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_float2s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2float(struct soap *soap, const char *s, float *p)
{ if (s)
  { if (!*s)
      return soap->error = SOAP_TYPE;
    if (!soap_tag_cmp(s, "INF"))
      *p = FLT_PINFTY;
    else if (!soap_tag_cmp(s, "+INF"))
      *p = FLT_PINFTY;
    else if (!soap_tag_cmp(s, "-INF"))
      *p = FLT_NINFTY;
    else if (!soap_tag_cmp(s, "NaN"))
      *p = FLT_NAN;
    else
    {
/* On some systems strtof requires -std=c99 or does not even link: so we try to use strtod first */
#if defined(WITH_C_LOCALE) && defined(HAVE_STRTOD_L)
      char *r;
# ifdef WIN32
      *p = (float)_strtod_l(s, &r, SOAP_LOCALE(soap));
# else
      *p = (float)strtod_l(s, &r, SOAP_LOCALE(soap));
# endif
      if (*r)
#elif defined(HAVE_STRTOD)
      char *r;
      *p = (float)strtod(s, &r);
      if (*r)
#elif defined(WITH_C_LOCALE) && defined(HAVE_STRTOF_L)
      char *r;
      *p = strtof_l((char*)s, &r, SOAP_LOCALE(soap));
      if (*r)
#elif defined(HAVE_STRTOF)
      char *r;
      *p = strtof((char*)s, &r);
      if (*r)
#endif
      {
#if defined(WITH_C_LOCALE) && defined(HAVE_SSCANF_L) && !defined(HAVE_STRTOF_L) && !defined(HAVE_STRTOD_L)
	double n;
        if (sscanf_l(s, SOAP_LOCALE(soap), "%lf", &n) != 1)
          soap->error = SOAP_TYPE;
	*p = (float)n;
#elif defined(HAVE_SSCANF)
	double n;
        if (sscanf(s, "%lf", &n) != 1)
          soap->error = SOAP_TYPE;
	*p = (float)n;
#else
        soap->error = SOAP_TYPE;
#endif
      }
    }
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
static int soap_isnumeric(struct soap *soap, const char *type)
{ if (soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":float")
   && soap_match_tag(soap, soap->type, ":double")
   && soap_match_tag(soap, soap->type, ":decimal")
   && soap_match_tag(soap, soap->type, ":integer")
   && soap_match_tag(soap, soap->type, ":positiveInteger")
   && soap_match_tag(soap, soap->type, ":negativeInteger")
   && soap_match_tag(soap, soap->type, ":nonPositiveInteger")
   && soap_match_tag(soap, soap->type, ":nonNegativeInteger")
   && soap_match_tag(soap, soap->type, ":long")
   && soap_match_tag(soap, soap->type, ":int")
   && soap_match_tag(soap, soap->type, ":short")
   && soap_match_tag(soap, soap->type, ":byte")
   && soap_match_tag(soap, soap->type, ":unsignedLong")
   && soap_match_tag(soap, soap->type, ":unsignedInt")
   && soap_match_tag(soap, soap->type, ":unsignedShort")
   && soap_match_tag(soap, soap->type, ":unsignedByte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return SOAP_ERR;
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
float *
SOAP_FMAC2
soap_infloat(struct soap *soap, const char *tag, float *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type != '\0' && soap_isnumeric(soap, type))
    return NULL;
#else
  (void)type;
#endif
  p = (float*)soap_id_enter(soap, soap->id, p, t, sizeof(float), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (float*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(float), 0, NULL, NULL);
  else if (p)
  { if (soap_s2float(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_double2s(struct soap *soap, double n)
{
#if !defined(WITH_C_LOCALE) || !defined(HAVE_SPRINTF_L)
  char *s;
#endif
  if (soap_isnan(n))
    return "NaN";
  if (soap_ispinfd(n))
    return "INF";
  if (soap_isninfd(n))
    return "-INF";
#if defined(WITH_C_LOCALE) && defined(HAVE_SPRINTF_L)
# ifdef WIN32
  _sprintf_s_l(soap->tmpbuf, _countof(soap->tmpbuf), soap->double_format, SOAP_LOCALE(soap), n);
# else
  sprintf_l(soap->tmpbuf, SOAP_LOCALE(soap), soap->double_format, n);
# endif
#else
  (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 40), soap->double_format, n);
  s = strchr(soap->tmpbuf, ',');	/* convert decimal comma to DP */
  if (s)
    *s = '.';
#endif
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outdouble(struct soap *soap, const char *tag, int id, const double *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_double2s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2double(struct soap *soap, const char *s, double *p)
{ if (s)
  { if (!*s)
      return soap->error = SOAP_TYPE;
    if (!soap_tag_cmp(s, "INF"))
      *p = DBL_PINFTY;
    else if (!soap_tag_cmp(s, "+INF"))
      *p = DBL_PINFTY;
    else if (!soap_tag_cmp(s, "-INF"))
      *p = DBL_NINFTY;
    else if (!soap_tag_cmp(s, "NaN"))
      *p = DBL_NAN;
    else
    {
#if defined(WITH_C_LOCALE) && defined(HAVE_STRTOD_L)
      char *r;
# ifdef WIN32
      *p = _strtod_l(s, &r, SOAP_LOCALE(soap));
# else
      *p = strtod_l(s, &r, SOAP_LOCALE(soap));
# endif
      if (*r)
#elif defined(HAVE_STRTOD)
      char *r;
      *p = strtod(s, &r);
      if (*r)
#endif
      {
#if defined(WITH_C_LOCALE) && defined(HAVE_SSCANF_L) && !defined(HAVE_STRTOF_L) && !defined(HAVE_STRTOD_L)
        if (sscanf_l(s, SOAP_LOCALE(soap), "%lf", p) != 1)
          soap->error = SOAP_TYPE;
#elif defined(HAVE_SSCANF)
        if (sscanf(s, "%lf", p) != 1)
          soap->error = SOAP_TYPE;
#else
        soap->error = SOAP_TYPE;
#endif
      }
    }
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
double *
SOAP_FMAC2
soap_indouble(struct soap *soap, const char *tag, double *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type != '\0' && soap_isnumeric(soap, type))
    return NULL;
#else
  (void)type;
#endif
  p = (double*)soap_id_enter(soap, soap->id, p, t, sizeof(double), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (double*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(double), 0, NULL, NULL);
  else if (p)
  { if (soap_s2double(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_unsignedByte2s(struct soap *soap, unsigned char n)
{ return soap_unsignedLong2s(soap, (unsigned long)n);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outunsignedByte(struct soap *soap, const char *tag, int id, const unsigned char *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_unsignedLong2s(soap, (unsigned long)*p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2unsignedByte(struct soap *soap, const char *s, unsigned char *p)
{ if (s)
  { unsigned long n;
    char *r;
    n = soap_strtoul(s, &r, 10);
    if (s == r || *r || n > 255)
      soap->error = SOAP_TYPE;
    *p = (unsigned char)n;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
unsigned char *
SOAP_FMAC2
soap_inunsignedByte(struct soap *soap, const char *tag, unsigned char *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":unsignedByte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (unsigned char*)soap_id_enter(soap, soap->id, p, t, sizeof(unsigned char), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (unsigned char*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(unsigned char), 0, NULL, NULL);
  else if (p)
  { if (soap_s2unsignedByte(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_unsignedShort2s(struct soap *soap, unsigned short n)
{ return soap_unsignedLong2s(soap, (unsigned long)n);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outunsignedShort(struct soap *soap, const char *tag, int id, const unsigned short *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_unsignedLong2s(soap, (unsigned long)*p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2unsignedShort(struct soap *soap, const char *s, unsigned short *p)
{ if (s)
  { unsigned long n;
    char *r;
    n = soap_strtoul(s, &r, 10);
    if (s == r || *r || n > 65535)
      soap->error = SOAP_TYPE;
    *p = (unsigned short)n;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
unsigned short *
SOAP_FMAC2
soap_inunsignedShort(struct soap *soap, const char *tag, unsigned short *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":unsignedShort")
   && soap_match_tag(soap, soap->type, ":unsignedByte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (unsigned short*)soap_id_enter(soap, soap->id, p, t, sizeof(unsigned short), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (unsigned short*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(unsigned short), 0, NULL, NULL);
  else if (p)
  { if (soap_s2unsignedShort(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_unsignedInt2s(struct soap *soap, unsigned int n)
{ return soap_unsignedLong2s(soap, (unsigned long)n);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outunsignedInt(struct soap *soap, const char *tag, int id, const unsigned int *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_unsignedLong2s(soap, (unsigned long)*p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2unsignedInt(struct soap *soap, const char *s, unsigned int *p)
{ if (s)
  { char *r;
#ifndef WITH_NOIO
#ifndef WITH_LEAN
    soap_reset_errno;
#endif
#endif
    *p = (unsigned int)soap_strtoul(s, &r, 10);
    if (s == r || *r
#ifndef WITH_NOIO
#ifndef WITH_LEAN
     || soap_errno == SOAP_ERANGE
#endif
#endif
    )
      soap->error = SOAP_TYPE;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
unsigned int *
SOAP_FMAC2
soap_inunsignedInt(struct soap *soap, const char *tag, unsigned int *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":unsignedInt")
   && soap_match_tag(soap, soap->type, ":unsignedShort")
   && soap_match_tag(soap, soap->type, ":unsignedByte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (unsigned int*)soap_id_enter(soap, soap->id, p, t, sizeof(unsigned int), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (unsigned int*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(unsigned int), 0, NULL, NULL);
  else if (p)
  { if (soap_s2unsignedInt(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_unsignedLong2s(struct soap *soap, unsigned long n)
{ (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 20), "%lu", n);
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outunsignedLong(struct soap *soap, const char *tag, int id, const unsigned long *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_unsignedLong2s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2unsignedLong(struct soap *soap, const char *s, unsigned long *p)
{ if (s)
  { char *r;
#ifndef WITH_NOIO
#ifndef WITH_LEAN
    soap_reset_errno;
#endif
#endif
    *p = soap_strtoul(s, &r, 10);
    if (s == r || *r
#ifndef WITH_NOIO
#ifndef WITH_LEAN
     || soap_errno == SOAP_ERANGE
#endif
#endif
    )
      soap->error = SOAP_TYPE;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
unsigned long *
SOAP_FMAC2
soap_inunsignedLong(struct soap *soap, const char *tag, unsigned long *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
#ifndef WITH_LEAN
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":unsignedInt")
   && soap_match_tag(soap, soap->type, ":unsignedShort")
   && soap_match_tag(soap, soap->type, ":unsignedByte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
#else
  (void)type;
#endif
  p = (unsigned long*)soap_id_enter(soap, soap->id, p, t, sizeof(unsigned long), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (unsigned long*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(unsigned long), 0, NULL, NULL);
  else if (p)
  { if (soap_s2unsignedLong(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_ULONG642s(struct soap *soap, ULONG64 n)
{ (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 20), SOAP_ULONG_FORMAT, n);
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outULONG64(struct soap *soap, const char *tag, int id, const ULONG64 *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_ULONG642s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2ULONG64(struct soap *soap, const char *s, ULONG64 *p)
{ if (s)
  { char *r;
#ifndef WITH_NOIO
#ifndef WITH_LEAN
    soap_reset_errno;
#endif
#endif
#ifdef HAVE_STRTOULL
    *p = soap_strtoull(s, &r, 10);
#else
    *p = (ULONG64)soap_strtoul(s, &r, 10);
#endif
    if (s == r || *r
#ifndef WITH_NOIO
#ifndef WITH_LEAN
       || soap_errno == SOAP_ERANGE
#endif
#endif
      )
      soap->error = SOAP_TYPE;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
ULONG64 *
SOAP_FMAC2
soap_inULONG64(struct soap *soap, const char *tag, ULONG64 *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":positiveInteger")
   && soap_match_tag(soap, soap->type, ":nonNegativeInteger")
   && soap_match_tag(soap, soap->type, ":unsignedLong")
   && soap_match_tag(soap, soap->type, ":unsignedInt")
   && soap_match_tag(soap, soap->type, ":unsignedShort")
   && soap_match_tag(soap, soap->type, ":unsignedByte"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
  p = (ULONG64*)soap_id_enter(soap, soap->id, p, t, sizeof(ULONG64), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (ULONG64*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(ULONG64), 0, NULL, NULL);
  else if (p)
  { if (soap_s2ULONG64(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2string(struct soap *soap, const char *s, char **t, long minlen, long maxlen)
{ if (s)
  { const char *r = soap_string(soap, s, minlen, maxlen);
    if (r && !(*t = soap_strdup(soap, r)))
      return soap->error = SOAP_EOM;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef WITH_COMPAT
#ifdef __cplusplus
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2std__string(struct soap *soap, const char *s, std::string *t, long minlen, long maxlen)
{ if (s)
  { const char *r = soap_string(soap, s, minlen, maxlen);
    if (r)
      t->assign(r);
  }
  return soap->error;
}
#endif
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
static const char*
soap_string(struct soap *soap, const char *s, long minlen, long maxlen)
{ if (s)
  { if (minlen > 0 || maxlen >= 0)
    { long l;
      if ((soap->mode & SOAP_C_UTFSTRING))
        l = (long)soap_utf8len(s);
      else
        l = (long)strlen(s);
      if (l > maxlen || l < minlen)
      { soap->error = SOAP_LENGTH;
	return NULL;
      }
    }
  }
  return s;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2QName(struct soap *soap, const char *s, char **t, long minlen, long maxlen)
{ if (s)
  { const char *r = soap_QName(soap, s, minlen, maxlen);
    if (r && !(*t = soap_strdup(soap, r)))
      return soap->error = SOAP_EOM;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef WITH_COMPAT
#ifdef __cplusplus
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2std__QName(struct soap *soap, const char *s, std::string *t, long minlen, long maxlen)
{ if (s)
  { const char *r = soap_QName(soap, s, minlen, maxlen);
    if (r)
      t->assign(r);
  }
  return soap->error;
}
#endif
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
static const char*
soap_QName(struct soap *soap, const char *s, long minlen, long maxlen)
{ if (s)
  {
#ifndef WITH_FAST
    char *b;
#endif
    if (minlen > 0 || maxlen >= 0)
    { long l;
      if ((soap->mode & SOAP_C_UTFSTRING))
        l = (long)soap_utf8len(s);
      else
        l = (long)strlen(s);
      if (l > maxlen || l < minlen)
      { soap->error = SOAP_LENGTH;
	return NULL;
      }
    }
#ifdef WITH_FAST
    soap->labidx = 0;
#else
    if (soap_new_block(soap) == NULL)
      return NULL;
#endif
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Normalized namespace(s) of QNames '%s'", s));
    /* convert (by prefix normalize prefix) all QNames in s */
    for (;;)
    { size_t n;
      struct soap_nlist *np;
      const char *p = NULL;
      short flag = 0;
      const char *r = NULL;
      size_t m = 0;
#ifndef WITH_FAST
      size_t k = 0;
#endif
      /* skip blanks */
      while (*s && soap_blank((soap_wchar)*s))
        s++;
      if (!*s)
        break;
      /* find next QName */
      n = 1;
      while (s[n] && !soap_blank((soap_wchar)s[n]))
        n++;
      np = soap->nlist;
      /* if there is no namespace stack, or prefix is "#" or "xml" then copy string */
      if (!np || *s == '#' || !strncmp(s, "xml:", 4))
      { r = s;
        m = n;
      }
      else /* we normalize the QName by replacing its prefix */
      { const char *q;
        for (p = s; *p && p < s + n; p++)
          if (*p == ':')
            break;
        if (*p == ':')
        { size_t k = p - s;
          while (np && (strncmp(np->id, s, k) || np->id[k]))
            np = np->next;
          p++;
        }
        else
        { while (np && *np->id)
            np = np->next;
          p = s;
        }
        /* replace prefix */
        if (np)
        { if (np->index >= 0 && soap->local_namespaces && (q = soap->local_namespaces[np->index].id))
          { size_t k = strlen(q);
            if (q[k-1] != '_')
            { r = q;
              m = k;
            }
            else
            { flag = 1; 
              r = soap->local_namespaces[np->index].ns;
              m = strlen(r);
            }
          }
          else if (np->ns)
          { flag = 1;
            r = np->ns;
            m = strlen(r);
          }
          else
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\nNamespace prefix of '%s' not defined (index=%d, URI='%s')\n", s, np->index, np->ns ? np->ns : SOAP_STR_EOS));
            soap->error = SOAP_NAMESPACE;
	    return NULL;
          }
        }
        else if (s[n]) /* no namespace, part of string */
        { r = s;
          m = n;
        }
        else /* no namespace: assume "" namespace */
        { flag = 1;
        }
      }
#ifdef WITH_FAST
      if ((flag && soap_append_lab(soap, "\"", 1))
       || (m && soap_append_lab(soap, r, m))
       || (flag && soap_append_lab(soap, "\"", 1))
       || (p && (soap_append_lab(soap, ":", 1) || soap_append_lab(soap, p, n - (p-s)))))
         return NULL;
#else
      k = 2*flag + m + (p ? n - (p-s) + 1 : 0) + (s[n] != '\0');
      b = (char*)soap_push_block(soap, NULL, k);
      if (!b)
        return NULL;
      if (flag)
        *b++ = '"';
      if (m)
      { if (soap_memcpy((void*)b, k, (const void*)r, m))
	{ soap->error = SOAP_EOM;
	  return NULL;
	}
        b += m;
      }
      if (flag)
        *b++ = '"';
      if (p)
      { *b++ = ':';
        if (soap_memcpy((void*)b, k - m - flag - 1, (const void*)p, n - (p-s)))
	{ soap->error = SOAP_EOM;
	  return NULL;
	}
        b += n - (p-s);
      }
#endif
      /* advance to next and add spacing */
      s += n;
      if (*s)
      {
#ifdef WITH_FAST
        if (soap_append_lab(soap, " ", 1))
	  return NULL;
#else
        *b = ' ';
#endif
      }
    }
#ifdef WITH_FAST
    if (soap_append_lab(soap, SOAP_STR_EOS, 1))
      return NULL;
    return soap->labbuf;
#else
    b = (char*)soap_push_block(soap, NULL, 1);
    if (!b)
      return NULL;
    *b = '\0';
    return (char*)soap_save_block(soap, NULL, NULL, 0);
#endif
  }
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_QName2s(struct soap *soap, const char *s)
{ const char *t = NULL;
  if (s)
  {
#ifdef WITH_FAST
    soap->labidx = 0;
#else
    char *b;
    if (soap_new_block(soap) == NULL)
      return NULL;
#endif
    for (;;)
    { size_t n;
      const char *q = NULL;
      const char *r = NULL;
      size_t m = 0;
#ifndef WITH_FAST
      size_t k = 0;
#endif
      /* skip blanks */
      while (*s && soap_blank((soap_wchar)*s))
        s++;
      if (!*s)
        break;
      /* find next QName */
      n = 0;
      while (s[n] && !soap_blank((soap_wchar)s[n]))
        n++;
      /* normal prefix: pass string as is */
      if (*s != '"')
      {
#ifndef WITH_LEAN
        if ((soap->mode & SOAP_XML_CANONICAL))
          soap_utilize_ns(soap, s);
        if ((soap->mode & SOAP_XML_DEFAULTNS))
        { r = strchr(s, ':');
          if (r && soap->nlist && !strncmp(soap->nlist->id, s, r - s) && !soap->nlist->id[r - s])
          { n -= r - s + 1;
            s = r + 1;
          }
        }
#endif
        r = s;
        m = n + 1;
      }
      else /* URL-based string prefix */
      { s++;
        q = strchr(s, '"');
        if (q)
        { struct Namespace *p = soap->local_namespaces;
          if (p)
          { for (; p->id; p++)
            { if (p->ns)
                if (!soap_tag_cmp(s, p->ns))
                  break;
              if (p->in)
                if (!soap_tag_cmp(s, p->in))
                  break;
            }
          }
	  q++;
          /* URL is in the namespace table? */
          if (p && p->id)
          { r = p->id;
#ifndef WITH_LEAN
            if ((soap->mode & SOAP_XML_DEFAULTNS) && soap->nlist && !strcmp(soap->nlist->id, r))
	      q++;
	    else
#endif
              m = strlen(r);
          }
          else /* not in namespace table: create xmlns binding */
          { char *x = soap_strdup(soap, s);
            x[q - s - 1] = '\0';
            (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 27), "xmlns:_%d", soap->idnum++);
            soap_set_attr(soap, soap->tmpbuf, x, 1);
            r = soap->tmpbuf + 6;
            m = strlen(r);
          }
        }
      }
      /* copy normalized QName into buffer, including the ending blank or NUL */
#ifdef WITH_FAST
      if ((m && soap_append_lab(soap, r, m))
       || (q && soap_append_lab(soap, q, n - (q - s))))
        return NULL;
#else
      k = m + (q ? n - (q - s) : 0);
      b = (char*)soap_push_block(soap, NULL, k);
      if (soap_memcpy((void*)b, k, (const void*)r, m))
      { soap->error = SOAP_EOM;
	return NULL;
      }
      b += m;
      if (q)
      { if (soap_memcpy((void*)b, k - m, (const void*)q, n - (q - s)))
	{ soap->error = SOAP_EOM;
	  return NULL;
	}
        b += n - (q - s);
      }
#endif
      /* advance to next */
      s += n;
    }
#ifdef WITH_FAST
    t = soap_strdup(soap, soap->labbuf);
#else
    t = (char*)soap_save_block(soap, NULL, NULL, 0);
#endif
  }
  return t;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2wchar(struct soap *soap, const char *s, wchar_t **t, long minlen, long maxlen)
{ if (s)
  { const wchar_t *r = soap_wstring(soap, s, minlen, maxlen);
    if (r && !(*t = soap_wstrdup(soap, r)))
      return soap->error = SOAP_EOM;
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef WITH_COMPAT
#ifdef __cplusplus
#ifndef WITH_LEAN
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2std__wstring(struct soap *soap, const char *s, std::wstring *t, long minlen, long maxlen)
{ if (s)
  { const wchar_t *r = soap_wstring(soap, s, minlen, maxlen);
    if (r)
      t->assign(r);
  }
  return soap->error;
}
#endif
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
static const wchar_t*
soap_wstring(struct soap *soap, const char *s, long minlen, long maxlen)
{ if (s)
  { long l;
    wchar_t wc;
    soap->labidx = 0;
    if (soap->mode & SOAP_ENC_LATIN)
    { wchar_t *r;
      if (soap_append_lab(soap, NULL, sizeof(wchar_t) * (strlen(s) + 1)))
        return NULL;
      r = (wchar_t*)soap->labbuf;
      while (*s)
        *r++ = (wchar_t)*s++;
    }
    else
    { /* Convert UTF8 to wchar */
      while (*s)
      { soap_wchar c, c1, c2, c3, c4;
        c = (unsigned char)*s++;
        if (c < 0x80)
          wc = (wchar_t)c;
        else
        { c1 = (soap_wchar)*s++ & 0x3F;
          if (c < 0xE0)
            wc = (wchar_t)(((soap_wchar)(c & 0x1F) << 6) | c1);
          else
          { c2 = (soap_wchar)*s++ & 0x3F;
            if (c < 0xF0)
              wc = (wchar_t)(((soap_wchar)(c & 0x0F) << 12) | (c1 << 6) | c2);
            else
            { c3 = (soap_wchar)*s++ & 0x3F;
              if (c < 0xF8)
                wc = (wchar_t)(((soap_wchar)(c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3);
              else
              { c4 = (soap_wchar)*s++ & 0x3F;
                if (c < 0xFC)
                  wc = (wchar_t)(((soap_wchar)(c & 0x03) << 24) | (c1 << 18) | (c2 << 12) | (c3 << 6) | c4);
                else
                  wc = (wchar_t)(((soap_wchar)(c & 0x01) << 30) | (c1 << 24) | (c2 << 18) | (c3 << 12) | (c4 << 6) | (soap_wchar)(*s++ & 0x3F));
              }
            }
          }
        }
	if (soap_append_lab(soap, (const char*)&wc, sizeof(wc)))
	  return NULL;
      }
    }
    l = (long)(soap->labidx / sizeof(wchar_t));
    wc = L'\0';
    if (soap_append_lab(soap, (const char*)&wc, sizeof(wc)))
      return NULL;
    if ((maxlen >= 0 && l > maxlen) || l < minlen)
    { soap->error = SOAP_LENGTH;
      return NULL;
    }
    return (wchar_t*)soap->labbuf;
  }
  return NULL;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_wchar2s(struct soap *soap, const wchar_t *s)
{ soap_wchar c;
  char *r, *t;
  const wchar_t *q = s;
  size_t n = 0;
  while ((c = *q++))
  { if (c > 0 && c < 0x80)
      n++;
    else
      n += 6;
  }
  r = t = (char*)soap_malloc(soap, n + 1);
  if (r)
  { /* Convert wchar to UTF8 */
    while ((c = *s++))
    { if (c > 0 && c < 0x80)
        *t++ = (char)c;
      else
      { if (c < 0x0800)
          *t++ = (char)(0xC0 | ((c >> 6) & 0x1F));
        else
        { if (c < 0x010000)
            *t++ = (char)(0xE0 | ((c >> 12) & 0x0F));
          else
          { if (c < 0x200000)
              *t++ = (char)(0xF0 | ((c >> 18) & 0x07));
            else
            { if (c < 0x04000000)
                *t++ = (char)(0xF8 | ((c >> 24) & 0x03));
              else
              { *t++ = (char)(0xFC | ((c >> 30) & 0x01));
                *t++ = (char)(0x80 | ((c >> 24) & 0x3F));
              }
              *t++ = (char)(0x80 | ((c >> 18) & 0x3F));
            }
            *t++ = (char)(0x80 | ((c >> 12) & 0x3F));
          }
          *t++ = (char)(0x80 | ((c >> 6) & 0x3F));
        }
        *t++ = (char)(0x80 | (c & 0x3F));
      }
    }
    *t = '\0';
  }
  return r;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outstring(struct soap *soap, const char *tag, int id, char *const*p, const char *type, int n)
{ id = soap_element_id(soap, tag, id, *p, NULL, 0, type, n, NULL);
  if (id < 0)
    return soap->error;
  if (!**p && (soap->mode & SOAP_C_NILSTRING))
    return soap_element_null(soap, tag, id, type);
  if (soap_element_begin_out(soap, tag, id, type)
   || soap_string_out(soap, *p, 0)
   || soap_element_end_out(soap, tag))
    return soap->error;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
char **
SOAP_FMAC2
soap_instring(struct soap *soap, const char *tag, char **p, const char *type, int t, int flag, long minlen, long maxlen, const char *pattern)
{ (void)type;
  if (soap_element_begin_in(soap, tag, 1, NULL))
  { if (!tag || *tag != '-' || soap->error != SOAP_NO_TAG)
      return NULL;
    soap->error = SOAP_OK;
  }
  if (!p)
  { if (!(p = (char**)soap_malloc(soap, sizeof(char*))))
      return NULL;
  }
  if (soap->null)
    *p = NULL;
  else if (soap->body)
  { *p = soap_string_in(soap, flag, minlen, maxlen, pattern);
    if (!*p || !(char*)soap_id_enter(soap, soap->id, *p, t, sizeof(char*), NULL, NULL, NULL, NULL))
      return NULL;
    if (!**p && tag && *tag == '-')
    { soap->error = SOAP_NO_TAG;
      return NULL;
    }
  }
  else if (tag && *tag == '-')
  { soap->error = SOAP_NO_TAG;
    return NULL;
  }
  else if (!*soap->href)
  { if (minlen > 0)
    { soap->error = SOAP_LENGTH;
      return NULL;
    }
    *p = soap_strdup(soap, SOAP_STR_EOS);
  }
  if (*soap->href)
    p = (char**)soap_id_lookup(soap, soap->href, (void**)p, t, sizeof(char**), 0, NULL);
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outwstring(struct soap *soap, const char *tag, int id, wchar_t *const*p, const char *type, int n)
{ id = soap_element_id(soap, tag, id, *p, NULL, 0, type, n, NULL);
  if (id < 0)
    return soap->error;
  if (!**p && (soap->mode & SOAP_C_NILSTRING))
    return soap_element_null(soap, tag, id, type);
  if (soap_element_begin_out(soap, tag, id, type)
   || soap_wstring_out(soap, *p, 0)
   || soap_element_end_out(soap, tag))
    return soap->error;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_2
SOAP_FMAC1
wchar_t **
SOAP_FMAC2
soap_inwstring(struct soap *soap, const char *tag, wchar_t **p, const char *type, int t, long minlen, long maxlen, const char *pattern)
{ (void)type;
  if (soap_element_begin_in(soap, tag, 1, NULL))
  { if (!tag || *tag != '-' || soap->error != SOAP_NO_TAG)
      return NULL;
    soap->error = SOAP_OK;
  }
  if (!p)
  { if (!(p = (wchar_t**)soap_malloc(soap, sizeof(wchar_t*))))
      return NULL;
  }
  if (soap->null)
    *p = NULL;
  else if (soap->body)
  { *p = soap_wstring_in(soap, 1, minlen, maxlen, pattern);
    if (!*p || !(wchar_t*)soap_id_enter(soap, soap->id, *p, t, sizeof(wchar_t*), NULL, NULL, NULL, NULL))
      return NULL;
    if (!**p && tag && *tag == '-')
    { soap->error = SOAP_NO_TAG;
      return NULL;
    }
  }
  else if (tag && *tag == '-')
  { soap->error = SOAP_NO_TAG;
    return NULL;
  }
  else if (!*soap->href)
  { if (minlen > 0)
    { soap->error = SOAP_LENGTH;
      return NULL;
    }
    *p = soap_wstrdup(soap, (wchar_t*)SOAP_STR_EOS);
  }
  if (*soap->href)
    p = (wchar_t**)soap_id_lookup(soap, soap->href, (void**)p, t, sizeof(wchar_t**), 0, NULL);
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifdef UNDER_CE
/* WinCE mktime (based on the mingw-runtime, public domain) */
#define __FILETIME_to_ll(f) ((long long)(f).dwHighDateTime << 32 | (long long)(f).dwLowDateTime)
static time_t
mktime(struct tm *pt)
{ SYSTEMTIME s, s1, s2;
  FILETIME f, f1, f2;
  long long diff;
  GetSystemTime(&s1);
  GetLocalTime(&s2);
  SystemTimeToFileTime(&s1, &f1);
  SystemTimeToFileTime(&s2, &f2);
  diff = (__FILETIME_to_ll(f2) - __FILETIME_to_ll(f1)) / 10000000LL;
  s.wYear = pt->tm_year + 1900;
  s.wMonth = pt->tm_mon  + 1;
  s.wDayOfWeek = pt->tm_wday;
  s.wDay = pt->tm_mday;
  s.wHour = pt->tm_hour;
  s.wMinute = pt->tm_min;
  s.wSecond = pt->tm_sec;
  s.wMilliseconds = 0;
  SystemTimeToFileTime(&s, &f);
  return (time_t)((__FILETIME_to_ll(f) - 116444736000000000LL) / 10000000LL) - (time_t)diff;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifdef UNDER_CE
/* WinCE gmtime_r (based on the mingw-runtime, public domain) */
#define HAVE_GMTIME_R
static struct tm*
gmtime_r(const time_t *t, struct tm *pt)
{ FILETIME f, f1, f2;
  SYSTEMTIME s, s1 = {0};
  long long time = (long long)(*t) * 10000000LL + 116444736000000000LL;
  f.dwHighDateTime = (DWORD)((time >> 32) & 0x00000000FFFFFFFF);
  f.dwLowDateTime = (DWORD)(time & 0x00000000FFFFFFFF);
  FileTimeToSystemTime(&f, &s);
  pt->tm_year = s.wYear - 1900;
  pt->tm_mon = s.wMonth - 1;
  pt->tm_wday = s.wDayOfWeek;
  pt->tm_mday = s.wDay;
  s1.wYear = s.wYear;
  s1.wMonth = 1;
  s1.wDayOfWeek = 1;
  s1.wDay = 1;
  SystemTimeToFileTime(&s1, &f1);
  SystemTimeToFileTime(&s, &f2);
  pt->tm_yday = (((__FILETIME_to_ll(f2) - __FILETIME_to_ll(f1)) / 10000000LL) / (60 * 60 * 24));
  pt->tm_hour = s.wHour;
  pt->tm_min = s.wMinute;
  pt->tm_sec = s.wSecond;
  pt->tm_isdst = 0;
  return pt;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifdef UNDER_CE
/* WinCE very simple strftime for format "%Y-%m-%dT%H:%M:%SZ", note: %F and %T not supported by MS */
static size_t
strftime(char *buf, size_t len, const char *format, const struct tm *pT)
{ (void)len; (void)format;
#ifndef WITH_NOZONE
  (SOAP_SNPRINTF(buf, len, 20), "%04d-%02d-%02dT%02d:%02d:%02dZ", pT->tm_year + 1900, pT->tm_mon + 1, pT->tm_mday, pT->tm_hour, pT->tm_min, pT->tm_sec);
#else
  (SOAP_SNPRINTF(buf, len, 20), "%04d-%02d-%02dT%02d:%02d:%02d", pT->tm_year + 1900, pT->tm_mon + 1, pT->tm_mday, pT->tm_hour, pT->tm_min, pT->tm_sec);
#endif
  return len;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
time_t
SOAP_FMAC2
soap_timegm(struct tm *T)
{
#if defined(HAVE_TIMEGM)
  return timegm(T);
#else
  time_t t, g, z;
  struct tm tm;
#ifndef HAVE_GMTIME_R
  struct tm *tp;
#endif
  t = mktime(T);
  if (t == (time_t)-1)
    return (time_t)-1;
#ifdef HAVE_GMTIME_R
  if (gmtime_r(&t, &tm) == SOAP_FUNC_R_ERR)
    return (time_t)-1;
#else
  tp = gmtime(&t);
  if (!tp)
    return (time_t)-1;
  tm = *tp;
#endif
  tm.tm_isdst = 0;
  g = mktime(&tm);
  if (g == (time_t)-1)
    return (time_t)-1;
  z = g - t;
  return t - z;
#endif
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_dateTime2s(struct soap *soap, time_t n)
{ struct tm T, *pT = &T;
#if defined(HAVE_GMTIME_R) && !defined(WITH_NOZONE)
  if (gmtime_r(&n, pT) != SOAP_FUNC_R_ERR)
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%SZ", pT);
#elif defined(HAVE_GMTIME) && !defined(WITH_NOZONE)
  if ((pT = gmtime(&n)))
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%SZ", pT);
#elif (defined(HAVE_TM_GMTOFF) || defined(HAVE_STRUCT_TM_TM_GMTOFF) || defined(HAVE_STRUCT_TM___TM_GMTOFF)) && !defined(WITH_NOZONE)
#if defined(HAVE_LOCALTIME_R)
  if (localtime_r(&n, pT) != SOAP_FUNC_R_ERR)
  { strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S%z", pT);
    soap_memmove(soap->tmpbuf + 23, sizeof(soap->tmpbuf) - 23, soap->tmpbuf + 22, 3); /* 2000-03-01T02:00:00+0300 */
    soap->tmpbuf[22] = ':';                                                           /* 2000-03-01T02:00:00+03:00 */
  }
#else
  if ((pT = localtime(&n)))
  { strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S%z", pT);
    soap_memmove(soap->tmpbuf + 23, sizeof(soap->tmpbuf) - 23, soap->tmpbuf + 22, 3); /* 2000-03-01T02:00:00+0300 */
    soap->tmpbuf[22] = ':';                                                           /* 2000-03-01T02:00:00+03:00 */
  }
#endif
#elif defined(HAVE_GETTIMEOFDAY) && !defined(WITH_NOZONE)
  struct timezone tz;
  memset((void*)&tz, 0, sizeof(tz));
#if defined(HAVE_LOCALTIME_R)
  if (localtime_r(&n, pT) != SOAP_FUNC_R_ERR)
  { struct timeval tv;
    size_t l;
    gettimeofday(&tv, &tz);
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S", pT);
    l = strlen(soap->tmpbuf);
    (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, 7), "%+03d:%02d", -tz.tz_minuteswest/60+(pT->tm_isdst!=0), abs(tz.tz_minuteswest)%60);
  }
#else
  if ((pT = localtime(&n)))
  { struct timeval tv;
    size_t l;
    gettimeofday(&tv, &tz);
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S", pT);
    l = strlen(soap->tmpbuf);
    (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, 7), "%+03d:%02d", -tz.tz_minuteswest/60+(pT->tm_isdst!=0), abs(tz.tz_minuteswest)%60);
  }
#endif
#elif defined(HAVE_FTIME) && !defined(WITH_NOZONE)
  struct timeb t;
  memset((void*)&t, 0, sizeof(t));
#if defined(HAVE_LOCALTIME_R)
  if (localtime_r(&n, pT) != SOAP_FUNC_R_ERR)
  { size_t l;
#ifdef __BORLANDC__
    ::ftime(&t);
#else
    ftime(&t);
#endif
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S", pT);
    l = strlen(soap->tmpbuf);
    (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, 7), "%+03d:%02d", -t.timezone/60+(pT->tm_isdst!=0), abs(t.timezone)%60);
  }
#else
  if ((pT = localtime(&n)))
  { size_t l;
#ifdef __BORLANDC__
    ::ftime(&t);
#else
    ftime(&t);
#endif
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S", pT);
    l = strlen(soap->tmpbuf);
    (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, 7), "%+03d:%02d", -t.timezone/60+(pT->tm_isdst!=0), abs(t.timezone)%60);
  }
#endif
#elif defined(HAVE_LOCALTIME_R)
  if (localtime_r(&n, pT) != SOAP_FUNC_R_ERR)
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S", pT);
#else
  if ((pT = localtime(&n)))
    strftime(soap->tmpbuf, sizeof(soap->tmpbuf), "%Y-%m-%dT%H:%M:%S", pT);
#endif
  else
    soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), "1969-12-31T23:59:59Z");
  return soap->tmpbuf;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
int
SOAP_FMAC2
soap_outdateTime(struct soap *soap, const char *tag, int id, const time_t *p, const char *type, int n)
{ if (soap_element_begin_out(soap, tag, soap_embedded_id(soap, id, p, n), type)
   || soap_string_out(soap, soap_dateTime2s(soap, *p), 0))
    return soap->error;
  return soap_element_end_out(soap, tag);
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
int
SOAP_FMAC2
soap_s2dateTime(struct soap *soap, const char *s, time_t *p)
{ *p = 0;
  if (s)
  { char *t;
    unsigned long d;
    struct tm T;
    memset((void*)&T, 0, sizeof(T));
    d = soap_strtoul(s, &t, 10);
    if (*t == '-')
    { /* YYYY-MM-DD */
      T.tm_year = (int)d;
      T.tm_mon = (int)soap_strtoul(t + 1, &t, 10);
      T.tm_mday = (int)soap_strtoul(t + 1, &t, 10);
    }
    else if (!(soap->mode & SOAP_XML_STRICT))
    { /* YYYYMMDD */
      T.tm_year = (int)(d / 10000);
      T.tm_mon = (int)(d / 100 % 100);
      T.tm_mday = (int)(d % 100);
    }
    else
      return soap->error = SOAP_TYPE;
    if (*t == 'T' || ((*t == 't' || *t == ' ') && !(soap->mode & SOAP_XML_STRICT)))
    { d = soap_strtoul(t + 1, &t, 10);
      if (*t == ':')
      { /* Thh:mm:ss */
	T.tm_hour = (int)d;
	T.tm_min = (int)soap_strtoul(t + 1, &t, 10);
	T.tm_sec = (int)soap_strtoul(t + 1, &t, 10);
      }
      else if (!(soap->mode & SOAP_XML_STRICT))
      { /* Thhmmss */
        T.tm_hour = (int)(d / 10000);
	T.tm_min = (int)(d / 100 % 100);
	T.tm_sec = (int)(d % 100);
      }
      else
	return soap->error = SOAP_TYPE;
    }
    if (T.tm_year == 1)
      T.tm_year = 70;
    else
      T.tm_year -= 1900;
    T.tm_mon--;
    if (*t == '.')
    { for (t++; *t; t++)
        if (*t < '0' || *t > '9')
          break;
    }
    if (*t == ' ' && !(soap->mode & SOAP_XML_STRICT))
      t++;
    if (*t)
    {
#ifndef WITH_NOZONE
      if (*t == '+' || *t == '-')
      { int h, m;
	m = (int)soap_strtol(t, &t, 10);
        if (*t == ':')
        { /* +hh:mm */
	  h = m;
	  m = (int)soap_strtol(t + 1, &t, 10);
          if (h < 0)
            m = -m;
        }
        else if (!(soap->mode & SOAP_XML_STRICT))
	{ /* +hhmm */
          h = m / 100;
          m = m % 100;
        }
        else
	{ /* +hh */
          h = m;
	  m = 0;
        }
	if (*t)
	  return soap->error = SOAP_TYPE;
        T.tm_min -= m;
        T.tm_hour -= h;
        /* put hour and min in range */
        T.tm_hour += T.tm_min / 60;
        T.tm_min %= 60;
        if (T.tm_min < 0)
        { T.tm_min += 60;
          T.tm_hour--;
        }
        T.tm_mday += T.tm_hour / 24;
        T.tm_hour %= 24;
        if (T.tm_hour < 0)
        { T.tm_hour += 24;
          T.tm_mday--;
        }
        /* note: day of the month may be out of range, timegm() handles it */
      }
      else if (*t != 'Z')
	return soap->error = SOAP_TYPE;
#endif
      *p = soap_timegm(&T);
    }
    else /* no UTC or timezone, so assume we got a localtime */
    { T.tm_isdst = -1;
      *p = mktime(&T);
    }
  }
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
time_t *
SOAP_FMAC2
soap_indateTime(struct soap *soap, const char *tag, time_t *p, const char *type, int t)
{ if (soap_element_begin_in(soap, tag, 0, NULL))
    return NULL;
  if (*soap->type
   && soap_match_tag(soap, soap->type, type)
   && soap_match_tag(soap, soap->type, ":dateTime"))
  { soap->error = SOAP_TYPE;
    soap_revert(soap);
    return NULL;
  }
  p = (time_t*)soap_id_enter(soap, soap->id, p, t, sizeof(time_t), NULL, NULL, NULL, NULL);
  if (*soap->href)
    p = (time_t*)soap_id_forward(soap, soap->href, p, 0, t, t, sizeof(time_t), 0, NULL, NULL);
  else if (p)
  { if (soap_s2dateTime(soap, soap_value(soap), p))
      return NULL;
  }
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outliteral(struct soap *soap, const char *tag, char *const*p, const char *type)
{ int i;
  const char *t = NULL;
  if (tag && *tag != '-')
  { if (soap->local_namespaces && (t = strchr(tag, ':')))
    { size_t n = t - tag;
      if (n >= sizeof(soap->tmpbuf))
        n = sizeof(soap->tmpbuf) - 1;
      soap_strncpy(soap->tmpbuf, sizeof(soap->tmpbuf), tag, n);
      for (i = 0; soap->local_namespaces[i].id; i++)
        if (!strcmp(soap->tmpbuf, soap->local_namespaces[i].id))
          break;
      t++;
      if (soap_element(soap, t, 0, type)
       || soap_attribute(soap, "xmlns", soap->local_namespaces[i].ns ? soap->local_namespaces[i].ns : SOAP_STR_EOS)
       || soap_element_start_end_out(soap, NULL))
        return soap->error;
    }
    else
    { t = tag;
      if (soap_element_begin_out(soap, t, 0, type))
        return soap->error;
    }
  }
  if (p && *p)
  { if (soap_send(soap, *p)) /* send as-is */
      return soap->error;
  }
  if (t)
    return soap_element_end_out(soap, t);
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
char **
SOAP_FMAC2
soap_inliteral(struct soap *soap, const char *tag, char **p)
{ if (soap_element_begin_in(soap, tag, 1, NULL))
  { if (soap->error != SOAP_NO_TAG || soap_unget(soap, soap_get(soap)) == SOAP_TT)
      return NULL;
    soap->error = SOAP_OK;
  }
  if (!p)
  { if (!(p = (char**)soap_malloc(soap, sizeof(char*))))
      return NULL;
  }
  if (soap->body || (tag && *tag == '-'))
  { *p = soap_string_in(soap, 0, -1, -1, NULL);
    if (!*p)
      return NULL;
    if (!**p && tag && *tag == '-')
    { soap->error = SOAP_NO_TAG;
      return NULL;
    }
  }
  else if (soap->null)
    *p = NULL;
  else
    *p = soap_strdup(soap, SOAP_STR_EOS);
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_outwliteral(struct soap *soap, const char *tag, wchar_t *const*p, const char *type)
{ int i;
  const char *t = NULL;
  if (tag && *tag != '-')
  { if (soap->local_namespaces && (t = strchr(tag, ':')))
    { size_t n = t - tag;
      if (n >= sizeof(soap->tmpbuf))
        n = sizeof(soap->tmpbuf) - 1;
      soap_strncpy(soap->tmpbuf, sizeof(soap->tmpbuf), tag, n);
      for (i = 0; soap->local_namespaces[i].id; i++)
        if (!strcmp(soap->tmpbuf, soap->local_namespaces[i].id))
          break;
      t++;
      if (soap_element(soap, t, 0, type)
       || soap_attribute(soap, "xmlns", soap->local_namespaces[i].ns ? soap->local_namespaces[i].ns : SOAP_STR_EOS)
       || soap_element_start_end_out(soap, NULL))
        return soap->error;
    }
    else
    { t = tag;
      if (soap_element_begin_out(soap, t, 0, type))
        return soap->error;
    }
  }
  if (p)
  { wchar_t c;
    const wchar_t *s = *p;
    while ((c = *s++))
    { if (soap_pututf8(soap, (unsigned long)c)) /* send as-is in UTF8 */
        return soap->error;
    }
  }
  if (t)
    return soap_element_end_out(soap, t);
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_2
SOAP_FMAC1
wchar_t **
SOAP_FMAC2
soap_inwliteral(struct soap *soap, const char *tag, wchar_t **p)
{ if (soap_element_begin_in(soap, tag, 1, NULL))
  { if (soap->error != SOAP_NO_TAG || soap_unget(soap, soap_get(soap)) == SOAP_TT)
      return NULL;
    soap->error = SOAP_OK;
  }
  if (!p)
  { if (!(p = (wchar_t**)soap_malloc(soap, sizeof(wchar_t*))))
      return NULL;
  }
  if (soap->body)
  { *p = soap_wstring_in(soap, 0, -1, -1, NULL);
    if (!*p)
      return NULL;
    if (!**p && tag && *tag == '-')
    { soap->error = SOAP_NO_TAG;
      return NULL;
    }
  }
  else if (tag && *tag == '-')
  { soap->error = SOAP_NO_TAG;
    return NULL;
  }
  else if (soap->null)
    *p = NULL;
  else
    *p = soap_wstrdup(soap, (wchar_t*)SOAP_STR_EOS);
  if (soap->body && soap_element_end_in(soap, tag))
    return NULL;
  return p;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
const char *
SOAP_FMAC2
soap_value(struct soap *soap)
{ size_t i;
  soap_wchar c = 0;
  char *s = soap->tmpbuf;
  if (!soap->body)
    return SOAP_STR_EOS;
  do c = soap_get(soap);
  while (soap_blank(c));
  for (i = 0; i < sizeof(soap->tmpbuf) - 1; i++)
  { if (c == SOAP_TT || c == SOAP_LT || (int)c == EOF)
      break;
    *s++ = (char)c;
    c = soap_get(soap);
  }
  for (s--; i > 0; i--, s--)
  { if (!soap_blank((soap_wchar)*s))
      break;
  }
  s[1] = '\0';
  soap->tmpbuf[sizeof(soap->tmpbuf) - 1] = '\0'; /* appease */
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Element content value='%s'\n", soap->tmpbuf));
  if (c == SOAP_TT || c == SOAP_LT || (int)c == EOF)
    soap_unget(soap, c);
  else
  { soap->error = SOAP_LENGTH;
    return NULL;
  }
#ifdef WITH_DOM
  if ((soap->mode & SOAP_XML_DOM) && soap->dom)
    soap->dom->data = soap_strdup(soap, soap->tmpbuf);
#endif
  return soap->tmpbuf; /* return non-null pointer */
}
#endif

/******************************************************************************/
#if !defined(WITH_LEANER) || !defined(WITH_NOHTTP)
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_getline(struct soap *soap, char *buf, int len)
{ char *s = buf;
  int i = len;
  soap_wchar c = 0;
  for (;;)
  { while (--i > 0)
    { c = soap_getchar(soap);
      if (c == '\r' || c == '\n')
        break;
      if ((int)c == EOF)
        return soap->error = SOAP_CHK_EOF;
      *s++ = (char)c;
    }
    *s = '\0';
    if (c != '\n')
      c = soap_getchar(soap); /* got \r or something else, now get \n */
    if (c == '\n')
    { if (i + 1 == len) /* empty line: end of HTTP/MIME header */
        break;
      c = soap_get0(soap);
      if (c != ' ' && c != '\t') /* HTTP line continuation? */
        break;
    }
    else if ((int)c == EOF)
      return soap->error = SOAP_CHK_EOF;
    if (i <= 0)
      return soap->error = SOAP_HDR;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
static size_t
soap_count_attachments(struct soap *soap)
{
#ifndef WITH_LEANER
  struct soap_multipart *content;
  size_t count = soap->count;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Calculating the message size with attachments, current count=%lu\n", (unsigned long)count));
  if ((soap->mode & SOAP_ENC_DIME) && !(soap->mode & SOAP_ENC_MTOM))
  { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Calculating the size of DIME attachments\n"));
    for (content = soap->dime.first; content; content = content->next)
    { count += 12 + ((content->size+3)&(~3));
      if (content->id)
        count += ((strlen(content->id)+3)&(~3));
      if (content->type)
        count += ((strlen(content->type)+3)&(~3));
      if (content->options)
        count += ((((unsigned char)content->options[2] << 8) | ((unsigned char)content->options[3]))+7)&(~3);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Size of DIME attachment content is %lu bytes\n", (unsigned long)content->size));
    }
  }
  if ((soap->mode & SOAP_ENC_MIME) && soap->mime.boundary)
  { size_t n = strlen(soap->mime.boundary);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Calculating the size of MIME attachments\n"));
    for (content = soap->mime.first; content; content = content->next)
    { const char *s;
      /* count \r\n--boundary\r\n */
      count += 6 + n;
      /* count Content-Type: ...\r\n */
      if (content->type)
        count += 16 + strlen(content->type);
      /* count Content-Transfer-Encoding: ...\r\n */
      s = soap_code_str(mime_codes, content->encoding);
      if (s)
        count += 29 + strlen(s);
      /* count Content-ID: ...\r\n */
      if (content->id)
        count += 14 + strlen(content->id);
      /* count Content-Location: ...\r\n */
      if (content->location)
        count += 20 + strlen(content->location);
      /* count Content-Description: ...\r\n */
      if (content->description)
        count += 23 + strlen(content->description);
      /* count \r\n...content */
      count += 2 + content->size;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Size of MIME attachment content is %lu bytes\n", (unsigned long)content->size));
    }
    /* count \r\n--boundary-- */
    count += 6 + n;
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New count=%lu\n", (unsigned long)count));
  return count;
#else
  return soap->count;
#endif
}
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
static int
soap_putdimefield(struct soap *soap, const char *s, size_t n)
{ if (soap_send_raw(soap, s, n))
    return soap->error;
  return soap_send_raw(soap, SOAP_STR_PADDING, -(long)n&3);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
char *
SOAP_FMAC2
soap_dime_option(struct soap *soap, unsigned short optype, const char *option)
{ size_t n;
  char *s = NULL;
  if (option)
  { n = strlen(option);
    s = (char*)soap_malloc(soap, n + 5);
    if (s)
    { s[0] = (char)(optype >> 8);
      s[1] = (char)(optype & 0xFF);
      s[2] = (char)(n >> 8);
      s[3] = (char)(n & 0xFF);
      soap_strcpy(s + 4, n + 1, option);
    }
  }
  return s;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_putdimehdr(struct soap *soap)
{ unsigned char tmp[12];
  size_t optlen = 0, idlen = 0, typelen = 0;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Put DIME header id='%s'\n", soap->dime.id ? soap->dime.id : SOAP_STR_EOS));
  if (soap->dime.options)
    optlen = (((unsigned char)soap->dime.options[2] << 8) | ((unsigned char)soap->dime.options[3])) + 4;
  if (soap->dime.id)
  { idlen = strlen(soap->dime.id);
    if (idlen > 0x0000FFFF)
      idlen = 0x0000FFFF;
  }
  if (soap->dime.type)
  { typelen = strlen(soap->dime.type);
    if (typelen > 0x0000FFFF)
      typelen = 0x0000FFFF;
  }
  tmp[0] = SOAP_DIME_VERSION | (soap->dime.flags & 0x7);
  tmp[1] = soap->dime.flags & 0xF0;
  tmp[2] = (char)(optlen >> 8);
  tmp[3] = (char)(optlen & 0xFF);
  tmp[4] = (char)(idlen >> 8);
  tmp[5] = (char)(idlen & 0xFF);
  tmp[6] = (char)(typelen >> 8);
  tmp[7] = (char)(typelen & 0xFF);
  tmp[8] = (char)(soap->dime.size >> 24);
  tmp[9] = (char)((soap->dime.size >> 16) & 0xFF);
  tmp[10] = (char)((soap->dime.size >> 8) & 0xFF);
  tmp[11] = (char)(soap->dime.size & 0xFF);
  if (soap_send_raw(soap, (char*)tmp, 12)
   || soap_putdimefield(soap, soap->dime.options, optlen)
   || soap_putdimefield(soap, soap->dime.id, idlen)
   || soap_putdimefield(soap, soap->dime.type, typelen))
    return soap->error;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_putdime(struct soap *soap)
{ struct soap_multipart *content;
  if (!(soap->mode & SOAP_ENC_DIME))
    return SOAP_OK;
  for (content = soap->dime.first; content; content = content->next)
  { void *handle;
    soap->dime.size = content->size;
    soap->dime.id = content->id;
    soap->dime.type = content->type;
    soap->dime.options = content->options;
    soap->dime.flags = SOAP_DIME_VERSION | SOAP_DIME_MEDIA;
    if (soap->fdimereadopen && ((handle = soap->fdimereadopen(soap, (void*)content->ptr, content->id, content->type, content->options)) || soap->error))
    { size_t size = content->size;
      if (!handle)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fdimereadopen failed\n"));
        return soap->error;
      }
      if (!size && ((soap->mode & SOAP_ENC_XML) || (soap->mode & SOAP_IO) == SOAP_IO_CHUNK || (soap->mode & SOAP_IO) == SOAP_IO_STORE))
      { size_t chunksize = sizeof(soap->tmpbuf);
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Chunked streaming DIME\n"));
        do
        { size = soap->fdimeread(soap, handle, soap->tmpbuf, chunksize);
          DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fdimeread returned %lu bytes\n", (unsigned long)size));
          if (size < chunksize)
          { soap->dime.flags &= ~SOAP_DIME_CF;
            if (!content->next)
              soap->dime.flags |= SOAP_DIME_ME;
          }
          else
            soap->dime.flags |= SOAP_DIME_CF;
          soap->dime.size = size;
          if (soap_putdimehdr(soap)
           || soap_putdimefield(soap, soap->tmpbuf, size))
            break;
          if (soap->dime.id)
          { soap->dime.flags &= ~(SOAP_DIME_MB | SOAP_DIME_MEDIA);
            soap->dime.id = NULL;
            soap->dime.type = NULL;
            soap->dime.options = NULL;
          }
        } while (size >= chunksize);
      }
      else
      { if (!content->next)
          soap->dime.flags |= SOAP_DIME_ME;
        if (soap_putdimehdr(soap))
          return soap->error;
        do
        { size_t bufsize;
          if (size < sizeof(soap->tmpbuf))
            bufsize = size;
          else
            bufsize = sizeof(soap->tmpbuf);
          if (!(bufsize = soap->fdimeread(soap, handle, soap->tmpbuf, bufsize)))
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fdimeread failed: insufficient data (%lu bytes remaining from %lu bytes)\n", (unsigned long)size, (unsigned long)content->size));
            soap->error = SOAP_CHK_EOF;
            break;
          }
          if (soap_send_raw(soap, soap->tmpbuf, bufsize))
            break;
          size -= bufsize;
        } while (size);
        DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fdimereadclose\n"));
        soap_send_raw(soap, SOAP_STR_PADDING, -(long)soap->dime.size&3);
      }
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fdimereadclose\n"));
      if (soap->fdimereadclose)
        soap->fdimereadclose(soap, handle);
    }
    else
    { if (!content->next)
        soap->dime.flags |= SOAP_DIME_ME;
      if (soap_putdimehdr(soap)
       || soap_putdimefield(soap, (char*)content->ptr, content->size))
        return soap->error;
    }
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
static char *
soap_getdimefield(struct soap *soap, size_t n)
{ soap_wchar c;
  size_t i;
  char *s;
  char *p = NULL;
  if (n)
  { p = (char*)soap_malloc(soap, n + 1);
    if (p)
    { s = p;
      for (i = n; i > 0; i--)
      { if ((int)(c = soap_get1(soap)) == EOF)
        { soap->error = SOAP_CHK_EOF;
          return NULL;
        }
        *s++ = (char)c;
      }
      *s = '\0'; /* force NUL terminated */
      if ((soap->error = soap_move(soap, (size_t)(-(long)n&3))))
        return NULL;
    }
    else
      soap->error = SOAP_EOM;
  }
  return p;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_getdimehdr(struct soap *soap)
{ soap_wchar c;
  char *s;
  int i;
  unsigned char tmp[12];
  size_t optlen, idlen, typelen;
  if (!(soap->mode & SOAP_ENC_DIME))
    return soap->error = SOAP_DIME_END;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get DIME header\n"));
  if (soap->dime.buflen || soap->dime.chunksize)
  { if (soap_move(soap, soap->dime.size - soap_tell(soap)))
      return soap->error = SOAP_CHK_EOF;
    soap_unget(soap, soap_getchar(soap)); /* skip padding and get hdr */
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "... From chunked\n"));
    return SOAP_OK;
  }
  s = (char*)tmp;
  for (i = 12; i > 0; i--)
  { if ((int)(c = soap_getchar(soap)) == EOF)
      return soap->error = SOAP_CHK_EOF;
    *s++ = (char)c;
  }
  if ((tmp[0] & 0xF8) != SOAP_DIME_VERSION)
    return soap->error = SOAP_DIME_MISMATCH;
  soap->dime.flags = (tmp[0] & 0x7) | (tmp[1] & 0xF0);
  optlen = (tmp[2] << 8) | tmp[3];
  idlen = (tmp[4] << 8) | tmp[5];
  typelen = (tmp[6] << 8) | tmp[7];
  soap->dime.size = ((size_t)tmp[8] << 24) | ((size_t)tmp[9] << 16) | ((size_t)tmp[10] << 8) | ((size_t)tmp[11]);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "DIME size=%lu flags=0x%X\n", (unsigned long)soap->dime.size, soap->dime.flags));
  if (!(soap->dime.options = soap_getdimefield(soap, optlen)) && soap->error)
    return soap->error;
  if (!(soap->dime.id = soap_getdimefield(soap, idlen)) && soap->error)
    return soap->error;
  if (!(soap->dime.type = soap_getdimefield(soap, typelen)) && soap->error)
    return soap->error;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "DIME id='%s', type='%s', options='%s'\n", soap->dime.id ? soap->dime.id : SOAP_STR_EOS, soap->dime.type ? soap->dime.type : "", soap->dime.options ? soap->dime.options+4 : SOAP_STR_EOS));
  if (soap->dime.flags & SOAP_DIME_ME)
    soap->mode &= ~SOAP_ENC_DIME;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_getdime(struct soap *soap)
{ while (soap->dime.flags & SOAP_DIME_CF)
  { if (soap_getdimehdr(soap))
      return soap->error;
    if (soap_move(soap, soap->dime.size))
      return soap->error = SOAP_EOF;
  }
  if (soap_move(soap, (size_t)(((soap->dime.size+3)&(~3)) - soap_tell(soap))))
    return soap->error = SOAP_EOF;
  for (;;)
  { struct soap_multipart *content;
    if (soap_getdimehdr(soap))
      break;
    if (soap->fdimewriteopen && ((soap->dime.ptr = (char*)soap->fdimewriteopen(soap, soap->dime.id, soap->dime.type, soap->dime.options)) || soap->error))
    { const char *id, *type, *options;
      size_t size, n;
      if (!soap->dime.ptr)
        return soap->error;
      id = soap->dime.id;
      type = soap->dime.type;
      options = soap->dime.options;
      for (;;)
      { size = soap->dime.size;
        for (;;)
        { n = soap->buflen - soap->bufidx;
          if (size < n)
            n = size;
          if ((soap->error = soap->fdimewrite(soap, (void*)soap->dime.ptr, soap->buf + soap->bufidx, n)))
            break;
          size -= n;
          if (!size)
          { soap->bufidx += n;
            break;
          }
          if (soap_recv(soap))
          { soap->error = SOAP_EOF;
            goto end;
          }
        }
        if (soap_move(soap, (size_t)(-(long)soap->dime.size&3)))
        { soap->error = SOAP_EOF;
          break;
        }
        if (!(soap->dime.flags & SOAP_DIME_CF))
          break;
        if (soap_getdimehdr(soap))
          break;
      }
end:
      if (soap->fdimewriteclose)
        soap->fdimewriteclose(soap, (void*)soap->dime.ptr);
      soap->dime.size = 0;
      soap->dime.id = id;
      soap->dime.type = type;
      soap->dime.options = options;
    }
    else if (soap->dime.flags & SOAP_DIME_CF)
    { const char *id, *type, *options;
      id = soap->dime.id;
      type = soap->dime.type;
      options = soap->dime.options;
      if (soap_new_block(soap) == NULL)
        return SOAP_EOM;
      for (;;)
      { soap_wchar c;
        size_t i;
        char *s;
        if (soap->dime.size > SOAP_MAXDIMESIZE)
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "DIME size=%lu exceeds SOAP_MAXDIMESIZE=%lu\n", (unsigned long)soap->dime.size, (unsigned long)SOAP_MAXDIMESIZE));
          return soap->error = SOAP_DIME_ERROR;
        }
        s = (char*)soap_push_block(soap, NULL, soap->dime.size);
        if (!s)
          return soap->error = SOAP_EOM;
        for (i = soap->dime.size; i > 0; i--)
        { if ((int)(c = soap_get1(soap)) == EOF)
            return soap->error = SOAP_EOF;
          *s++ = (char)c;
        }
        if (soap_move(soap, (size_t)(-(long)soap->dime.size&3)))
          return soap->error = SOAP_EOF;
        if (!(soap->dime.flags & SOAP_DIME_CF))
          break;
        if (soap_getdimehdr(soap))
          return soap->error;
      }
      soap->dime.size = soap->blist->size++; /* allocate one more byte in blist for the terminating '\0' */
      if (!(soap->dime.ptr = soap_save_block(soap, NULL, NULL, 0)))
        return soap->error;
      soap->dime.ptr[soap->dime.size] = '\0'; /* make 0-terminated */
      soap->dime.id = id;
      soap->dime.type = type;
      soap->dime.options = options;
    }
    else
      soap->dime.ptr = soap_getdimefield(soap, soap->dime.size);
    content = soap_new_multipart(soap, &soap->dime.first, &soap->dime.last, soap->dime.ptr, soap->dime.size);
    if (!content)
      return soap->error = SOAP_EOM;
    content->id = soap->dime.id;
    content->type = soap->dime.type;
    content->options = soap->dime.options;
    if (soap->error)
      return soap->error;
    soap_resolve_attachment(soap, content);
  }
  if (soap->error != SOAP_DIME_END)
    return soap->error;
  return soap->error = SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_getmimehdr(struct soap *soap)
{ struct soap_multipart *content;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get MIME header\n"));
  do
  { if (soap_getline(soap, soap->msgbuf, sizeof(soap->msgbuf)))
      return soap->error;
  } while (!*soap->msgbuf);
  if (soap->msgbuf[0] == '-' && soap->msgbuf[1] == '-')
  { char *s = soap->msgbuf + strlen(soap->msgbuf) - 1;
    /* remove white space */
    while (soap_blank((soap_wchar)*s))
      s--;
    s[1] = '\0';
    if (soap->mime.boundary)
    { if (strcmp(soap->msgbuf + 2, soap->mime.boundary))
        return soap->error = SOAP_MIME_ERROR;
    }
    else
      soap->mime.boundary = soap_strdup(soap, soap->msgbuf + 2);
    if (soap_getline(soap, soap->msgbuf, sizeof(soap->msgbuf)))
      return soap->error;
  }
  if (soap_set_mime_attachment(soap, NULL, 0, SOAP_MIME_NONE, NULL, NULL, NULL, NULL))
    return soap->error = SOAP_EOM;
  content = soap->mime.last;
  for (;;)
  { char *key = soap->msgbuf;
    char *val;
    if (!*key)
      break;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "MIME header: %s\n", key));
    val = strchr(soap->msgbuf, ':');
    if (val)
    { *val = '\0';
      do val++;
      while (*val && *val <= 32);
      if (!soap_tag_cmp(key, "Content-ID"))
        content->id = soap_strdup(soap, val);
      else if (!soap_tag_cmp(key, "Content-Location"))
        content->location = soap_strdup(soap, val);
      else if (!content->id && !soap_tag_cmp(key, "Content-Disposition"))
        content->id = soap_strdup(soap, soap_get_header_attribute(soap, val, "name"));
      else if (!soap_tag_cmp(key, "Content-Type"))
        content->type = soap_strdup(soap, val);
      else if (!soap_tag_cmp(key, "Content-Description"))
        content->description = soap_strdup(soap, val);
      else if (!soap_tag_cmp(key, "Content-Transfer-Encoding"))
        content->encoding = (enum soap_mime_encoding)soap_code_int(mime_codes, val, (LONG64)SOAP_MIME_NONE);
    }
    if (soap_getline(soap, key, sizeof(soap->msgbuf)))
      return soap->error;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_getmime(struct soap *soap)
{ while (soap_get_mime_attachment(soap, NULL))
    continue;
  return soap->error;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_post_check_mime_attachments(struct soap *soap)
{ soap->imode |= SOAP_MIME_POSTCHECK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_check_mime_attachments(struct soap *soap)
{ if (soap->mode & SOAP_MIME_POSTCHECK)
    return soap_get_mime_attachment(soap, NULL) != NULL;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
struct soap_multipart *
SOAP_FMAC2
soap_get_mime_attachment(struct soap *soap, void *handle)
{ soap_wchar c = 0;
  size_t i, m = 0;
  char *s, *t = NULL;
  struct soap_multipart *content;
  short flag = 0;
  if (!(soap->mode & SOAP_ENC_MIME))
    return NULL;
  content = soap->mime.last;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get MIME (%p)\n", content));
  if (!content)
  { if (soap_getmimehdr(soap))
      return NULL;
    content = soap->mime.last;
  }
  else if (content != soap->mime.first)
  { if (soap->fmimewriteopen && ((content->ptr = (char*)soap->fmimewriteopen(soap, (void*)handle, content->id, content->type, content->description, content->encoding)) || soap->error))
    { if (!content->ptr)
        return NULL;
    }
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Parsing MIME content id='%s' type='%s'\n", content->id ? content->id : SOAP_STR_EOS, content->type ? content->type : SOAP_STR_EOS));
  if (!content->ptr && soap_new_block(soap) == NULL)
  { soap->error = SOAP_EOM;
    return NULL;
  }
  for (;;)
  { if (content->ptr)
      s = soap->tmpbuf;
    else if (!(s = (char*)soap_push_block(soap, NULL, sizeof(soap->tmpbuf))))
    { soap->error = SOAP_EOM;
      return NULL;
    }
    for (i = 0; i < sizeof(soap->tmpbuf); i++)
    { if (m > 0)
      { *s++ = *t++;
        m--;
      }
      else
      { if (!flag)
        { c = soap_getchar(soap);
          if ((int)c == EOF)
          { if (content->ptr && soap->fmimewriteclose)
              soap->fmimewriteclose(soap, (void*)content->ptr);
            soap->error = SOAP_CHK_EOF;
            return NULL;
          }
        }
        if (flag || c == '\r')
        { memset((void*)soap->msgbuf, 0, sizeof(soap->msgbuf));
          soap_strcpy(soap->msgbuf, sizeof(soap->msgbuf), "\n--");
          if (soap->mime.boundary)
            soap_strncat(soap->msgbuf, sizeof(soap->msgbuf), soap->mime.boundary, sizeof(soap->msgbuf) - 4);
          t = soap->msgbuf;
          do c = soap_getchar(soap);
          while (c == *t++);
          if ((int)c == EOF)
          { if (content->ptr && soap->fmimewriteclose)
              soap->fmimewriteclose(soap, (void*)content->ptr);
            soap->error = SOAP_CHK_EOF;
            return NULL;
          }
          if (!*--t)
            goto end;
          *t = (char)c;
          flag = (c == '\r');
          m = t - soap->msgbuf + 1 - flag;
          t = soap->msgbuf;
          c = '\r';
        }
        *s++ = (char)c;
      }
    }
    if (content->ptr && soap->fmimewrite)
    { if ((soap->error = soap->fmimewrite(soap, (void*)content->ptr, soap->tmpbuf, i)))
        break;
    }
  }
end:
  *s = '\0'; /* make 0-terminated */
  if (content->ptr)
  { if (!soap->error && soap->fmimewrite)
      soap->error = soap->fmimewrite(soap, (void*)content->ptr, soap->tmpbuf, i);
    if (soap->fmimewriteclose)
      soap->fmimewriteclose(soap, (void*)content->ptr);
    if (soap->error)
      return NULL;
  }
  else
  { content->size = soap_size_block(soap, NULL, i+1) - 1; /* last block with '\0' */
    content->ptr = soap_save_block(soap, NULL, NULL, 0);
  }
  soap_resolve_attachment(soap, content);
  if (c == '-' && soap_getchar(soap) == '-')
  { soap->mode &= ~SOAP_ENC_MIME;
    if ((soap->mode & SOAP_MIME_POSTCHECK) && soap_end_recv(soap))
    { if (soap->keep_alive < 0)
        soap->keep_alive = 0;
      soap_closesock(soap);
      return NULL;
    }
  }
  else
  { while (c != '\r' && (int)c != EOF && soap_blank(c))
      c = soap_getchar(soap);
    if (c != '\r' || soap_getchar(soap) != '\n')
    { soap->error = SOAP_MIME_ERROR;
      return NULL;
    }
    if (soap_getmimehdr(soap))
      return NULL;
  }
  return content;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_match_cid(struct soap *soap, const char *s, const char *t)
{ size_t n;
  if (!s)
    return 1;
  if (!strcmp(s, t))
    return 0;
  if (!strncmp(s, "cid:", 4))
    s += 4;
  n = strlen(t);
  if (*t == '<')
  { t++;
    n -= 2;
  }
  if (!strncmp(s, t, n) && !s[n])
    return 0;
  soap_decode(soap->tmpbuf, sizeof(soap->tmpbuf), s, SOAP_STR_EOS);
  if (!strncmp(soap->tmpbuf, t, n) && !soap->tmpbuf[n])
    return 0;
  return 1;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
static void
soap_resolve_attachment(struct soap *soap, struct soap_multipart *content)
{ if (content->id)
  { struct soap_xlist **xp = &soap->xlist;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Resolving attachment data for id='%s'\n", content->id));
    while (*xp)
    { struct soap_xlist *xq = *xp;
      if (!soap_match_cid(soap, xq->id, content->id))
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Found matching attachment id='%s' for content id='%s'\n", xq->id, content->id));
        *xp = xq->next;
        *xq->ptr = (unsigned char*)content->ptr;
        *xq->size = (int)content->size;
        *xq->type = (char*)content->type;
        if (content->options)
          *xq->options = (char*)content->options;
        else
          *xq->options = (char*)content->description;
        SOAP_FREE(soap, xq);
      }
      else
        xp = &(*xp)->next;
    }
  }
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_putmimehdr(struct soap *soap, struct soap_multipart *content)
{ const char *s;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "MIME attachment type='%s'\n", content->type ? content->type : SOAP_STR_EOS));
  if (soap_send3(soap, "\r\n--", soap->mime.boundary, "\r\n"))
    return soap->error;
  if (content->type && soap_send3(soap, "Content-Type: ", content->type, "\r\n"))
    return soap->error;
  s = soap_code_str(mime_codes, content->encoding);
  if (s && soap_send3(soap, "Content-Transfer-Encoding: ", s, "\r\n"))
    return soap->error;
  if (content->id && soap_send3(soap, "Content-ID: ", content->id, "\r\n"))
    return soap->error;
  if (content->location && soap_send3(soap, "Content-Location: ", content->location, "\r\n"))
    return soap->error;
  if (content->description && soap_send3(soap, "Content-Description: ", content->description, "\r\n"))
    return soap->error;
  return soap_send_raw(soap, "\r\n", 2);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_putmime(struct soap *soap)
{ struct soap_multipart *content;
  if (!(soap->mode & SOAP_ENC_MIME) || !soap->mime.boundary)
    return SOAP_OK;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Sending MIME attachments\n"));
  for (content = soap->mime.first; content; content = content->next)
  { void *handle;
    if (soap->fmimereadopen && ((handle = soap->fmimereadopen(soap, (void*)content->ptr, content->id, content->type, content->description)) || soap->error))
    { size_t size = content->size;
      if (!handle)
      { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fmimereadopen failed\n"));
        return soap->error;
      }
      if (soap_putmimehdr(soap, content))
        return soap->error;
      if (!size)
      { if ((soap->mode & SOAP_ENC_XML) || (soap->mode & SOAP_IO) == SOAP_IO_CHUNK || (soap->mode & SOAP_IO) == SOAP_IO_STORE)
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Chunked streaming MIME\n"));
          do
          { size = soap->fmimeread(soap, handle, soap->tmpbuf, sizeof(soap->tmpbuf));
            DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fmimeread returned %lu bytes\n", (unsigned long)size));
            if (soap_send_raw(soap, soap->tmpbuf, size))
              break;
          } while (size);
        }
        else
        { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Error: cannot chunk streaming MIME (no HTTP chunking)\n"));
        }
      }
      else
      { do
        { size_t bufsize;
          if (size < sizeof(soap->tmpbuf))
            bufsize = size;
          else
            bufsize = sizeof(soap->tmpbuf);
          if (!(bufsize = soap->fmimeread(soap, handle, soap->tmpbuf, bufsize)))
          { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "fmimeread failed: insufficient data (%lu bytes remaining from %lu bytes)\n", (unsigned long)size, (unsigned long)content->size));
            soap->error = SOAP_EOF;
            break;
          }
          if (soap_send_raw(soap, soap->tmpbuf, bufsize))
            break;
          size -= bufsize;
        } while (size);
      }
      if (soap->fmimereadclose)
        soap->fmimereadclose(soap, handle);
    }
    else
    { if (soap_putmimehdr(soap, content)
       || soap_send_raw(soap, content->ptr, content->size))
        return soap->error;
    }
  }
  return soap_send3(soap, "\r\n--", soap->mime.boundary, "--");
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_dime(struct soap *soap)
{ soap->omode |= SOAP_ENC_DIME;
  soap->dime.first = NULL;
  soap->dime.last = NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_mime(struct soap *soap, const char *boundary, const char *start)
{ soap->omode |= SOAP_ENC_MIME;
  soap->mime.first = NULL;
  soap->mime.last = NULL;
  soap->mime.boundary = soap_strdup(soap, boundary);
  soap->mime.start = soap_strdup(soap, start);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_clr_dime(struct soap *soap)
{ soap->omode &= ~SOAP_ENC_DIME;
  soap->dime.first = NULL;
  soap->dime.last = NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_clr_mime(struct soap *soap)
{ soap->omode &= ~SOAP_ENC_MIME;
  soap->mime.first = NULL;
  soap->mime.last = NULL;
  soap->mime.boundary = NULL;
  soap->mime.start = NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
static struct soap_multipart*
soap_new_multipart(struct soap *soap, struct soap_multipart **first, struct soap_multipart **last, char *ptr, size_t size)
{ struct soap_multipart *content;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "New MIME attachment %p (%lu)\n", ptr, (unsigned long)size));
  content = (struct soap_multipart*)soap_malloc(soap, sizeof(struct soap_multipart));
  if (content)
  { content->next = NULL;
    content->ptr = ptr;
    content->size = size;
    content->id = NULL;
    content->type = NULL;
    content->options = NULL;
    content->encoding = SOAP_MIME_NONE;
    content->location = NULL;
    content->description = NULL;
    if (!*first)
      *first = content;
    if (*last)
      (*last)->next = content;
    *last = content;
  }
  return content;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_dime_attachment(struct soap *soap, char *ptr, size_t size, const char *type, const char *id, unsigned short optype, const char *option)
{ struct soap_multipart *content = soap_new_multipart(soap, &soap->dime.first, &soap->dime.last, ptr, size);
  if (!content)
    return SOAP_EOM;
  content->id = soap_strdup(soap, id);
  content->type = soap_strdup(soap, type);
  content->options = soap_dime_option(soap, optype, option);
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_mime_attachment(struct soap *soap, char *ptr, size_t size, enum soap_mime_encoding encoding, const char *type, const char *id, const char *location, const char *description)
{ struct soap_multipart *content = soap_new_multipart(soap, &soap->mime.first, &soap->mime.last, ptr, size);
  if (!content)
    return SOAP_EOM;
  content->id = soap_strdup(soap, id);
  content->type = soap_strdup(soap, type);
  content->encoding = encoding;
  content->location = soap_strdup(soap, location);
  content->description = soap_strdup(soap, description);
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
SOAP_FMAC1
struct soap_multipart*
SOAP_FMAC2
soap_next_multipart(struct soap_multipart *content)
{ if (content)
    return content->next;
  return NULL;
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
static void
soap_select_mime_boundary(struct soap *soap)
{ while (!soap->mime.boundary || soap_valid_mime_boundary(soap))
  { char *s = soap->mime.boundary;
    size_t n = 0;
    if (s)
      n = strlen(s);
    if (n < 16)
    { n = 64;
      s = soap->mime.boundary = (char*)soap_malloc(soap, n + 1);
      if (!s)
        return;
    }
    *s++ = '=';
    *s++ = '=';
    n -= 4;
    while (n)
    { *s++ = soap_base64o[soap_random & 0x3F];
      n--;
    }
    *s++ = '=';
    *s++ = '=';
    *s = '\0';
  }
  if (!soap->mime.start)
    soap->mime.start = "<SOAP-ENV:Envelope>";
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEANER
#ifndef PALM_1
static int
soap_valid_mime_boundary(struct soap *soap)
{ struct soap_multipart *content;
  size_t k;
  if (soap->fmimeread)
    return SOAP_OK;
  k = strlen(soap->mime.boundary);
  for (content = soap->mime.first; content; content = content->next)
  { if (content->ptr && content->size >= k)
    { const char *p = (const char*)content->ptr;
      size_t i;
      for (i = 0; i < content->size - k; i++, p++)
      { if (!strncmp(p, soap->mime.boundary, k))
          return SOAP_ERR;
      }
    }
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifdef WITH_GZIP
#ifndef PALM_1
static int
soap_getgziphdr(struct soap *soap)
{ int i;
  soap_wchar c = 0, f = 0;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Get gzip header\n"));
  for (i = 0; i < 9; i++)
  { if ((int)(c = soap_get1(soap) == EOF))
      return soap->error = SOAP_ZLIB_ERROR;
    if (i == 1 && c == 8)
      soap->z_dict = 0;
    if (i == 2)
      f = c;
  }
  if (f & 0x04) /* FEXTRA */
  { for (i = soap_get1(soap) | (soap_get1(soap) << 8); i; i--)
    { if ((int)soap_get1(soap) == EOF)
        return soap->error = SOAP_ZLIB_ERROR;
    }
  }
  if (f & 0x08) /* skip FNAME */
  { do
      c = soap_get1(soap);
    while (c && (int)c != EOF);
  }
  if ((int)c != EOF && (f & 0x10)) /* skip FCOMMENT */
  { do
      c = soap_get1(soap);
    while (c && (int)c != EOF);
  }
  if ((int)c != EOF && (f & 0x02)) /* skip FHCRC (CRC32 is used) */
  { if ((int)(c = soap_get1(soap)) != EOF)
      c = soap_get1(soap);
  }
  if ((int)c == EOF)
    return soap->error = SOAP_ZLIB_ERROR;
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_begin_serve(struct soap *soap)
{
#ifdef WITH_FASTCGI
  if (FCGI_Accept() < 0)
  { soap->error = SOAP_EOF;
    return soap_send_fault(soap);
  }
#endif
  soap_begin(soap);
  if (soap_begin_recv(soap)
   || soap_envelope_begin_in(soap)
   || soap_recv_header(soap)
   || soap_body_begin_in(soap))
  { if (soap->error < SOAP_STOP)
    {
#ifdef WITH_FASTCGI
      soap_send_fault(soap);
#else
      return soap_send_fault(soap);
#endif
    }
    return soap_closesock(soap);
  }
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_begin_recv(struct soap *soap)
{ soap_wchar c;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Initializing for input from socket=%d/fd=%d\n", soap->socket, soap->recvfd));
  soap->error = SOAP_OK;
#ifndef WITH_LEANER
  soap->recverror = SOAP_OK;
#endif
  soap_free_temp(soap);
  soap_set_local_namespaces(soap);
  soap->version = 0;	/* don't assume we're parsing SOAP content by default */
  soap_free_iht(soap);
  if ((soap->imode & SOAP_IO) == SOAP_IO_CHUNK)
    soap->omode |= SOAP_IO_CHUNK;
  soap->imode &= ~(SOAP_IO | SOAP_ENC_MIME);
  soap->mode = soap->imode;
  if (!soap->keep_alive)
  { soap->buflen = 0;
    soap->bufidx = 0;
  }
  if (!(soap->mode & SOAP_IO_KEEPALIVE))
    soap->keep_alive = 0;
  soap->shaky = 0;
  soap->ahead = 0;
  soap->peeked = 0;
  soap->level = 0;
  soap->part = SOAP_BEGIN;
  soap->body = 1;
  soap->count = 0;
  soap->length = 0;
  soap->cdata = 0;
  *soap->endpoint = '\0';
  soap->action = NULL;
  soap->header = NULL;
  soap->fault = NULL;
  soap->status = 0;
  soap->fform = NULL;
#ifndef WITH_LEANER
  soap->dom = NULL;
  soap->dime.chunksize = 0;
  soap->dime.buflen = 0;
  soap->dime.list = NULL;
  soap->dime.first = NULL;
  soap->dime.last = NULL;
  soap->mime.list = NULL;
  soap->mime.first = NULL;
  soap->mime.last = NULL;
  soap->mime.boundary = NULL;
  soap->mime.start = NULL;
#endif
#ifdef WIN32
#ifndef UNDER_CE
#ifndef WITH_FASTCGI
  if (!soap_valid_socket(soap->socket) && !soap->is) /* Set win32 stdout or soap->sendfd to BINARY, e.g. to support DIME */
#ifdef __BORLANDC__
    setmode(soap->recvfd, _O_BINARY);
#else
    _setmode(soap->recvfd, _O_BINARY);
#endif
#endif
#endif
#endif
#ifdef WITH_ZLIB
  soap->mode &= ~SOAP_ENC_ZLIB;
  soap->zlib_in = SOAP_ZLIB_NONE;
  soap->zlib_out = SOAP_ZLIB_NONE;
  if (!soap->d_stream)
  { soap->d_stream = (z_stream*)SOAP_MALLOC(soap, sizeof(z_stream));
    soap->d_stream->zalloc = Z_NULL;
    soap->d_stream->zfree = Z_NULL;
    soap->d_stream->opaque = Z_NULL;
    soap->d_stream->next_in = Z_NULL;
  }
  soap->d_stream->avail_in = 0;
  soap->d_stream->next_out = (Byte*)soap->buf;
  soap->d_stream->avail_out = sizeof(soap->buf);
  soap->z_ratio_in = 1.0;
#endif
#ifdef WITH_OPENSSL
  if (soap->ssl)
    ERR_clear_error();
#endif
#ifndef WITH_LEANER
  if (soap->fprepareinitrecv && (soap->error = soap->fprepareinitrecv(soap)))
    return soap->error;
#endif
  c = soap_getchar(soap);
#ifdef WITH_GZIP
  if (c == 0x1F)
  { if (soap_getgziphdr(soap))
      return soap->error;
    if (inflateInit2(soap->d_stream, -MAX_WBITS) != Z_OK)
      return soap->error = SOAP_ZLIB_ERROR;
    if (soap->z_dict)
    { if (inflateSetDictionary(soap->d_stream, (const Bytef*)soap->z_dict, soap->z_dict_len) != Z_OK)
        return soap->error = SOAP_ZLIB_ERROR;
    }
    soap->zlib_state = SOAP_ZLIB_INFLATE;
    soap->mode |= SOAP_ENC_ZLIB;
    soap->zlib_in = SOAP_ZLIB_GZIP;
    soap->z_crc = crc32(0L, NULL, 0);
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "gzip initialized\n"));
    if (!soap->z_buf)
      soap->z_buf = (char*)SOAP_MALLOC(soap, sizeof(soap->buf));
    soap_memcpy((void*)soap->z_buf, sizeof(soap->buf), (const void*)soap->buf, sizeof(soap->buf));
    /* should not chunk over plain transport, so why bother to check? */
    /* if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK) */
    /*   soap->z_buflen = soap->bufidx; */
    /* else */
    soap->d_stream->next_in = (Byte*)(soap->z_buf + soap->bufidx);
    soap->d_stream->avail_in = (unsigned int)(soap->buflen - soap->bufidx);
    soap->z_buflen = soap->buflen;
    soap->buflen = soap->bufidx;
    c = ' ';
  }
#endif
  while (soap_blank(c))
    c = soap_getchar(soap);
#ifndef WITH_LEANER
  if (c == '-' && soap_get0(soap) == '-')
    soap->mode |= SOAP_ENC_MIME;
  else if ((c & 0xFFFC) == (SOAP_DIME_VERSION | SOAP_DIME_MB) && (soap_get0(soap) & 0xFFF0) == 0x20)
    soap->mode |= SOAP_ENC_DIME;
  else
#endif
  { /* skip BOM */
    if (c == 0xEF && soap_get0(soap) == 0xBB)
    { c = soap_get1(soap);
      if ((c = soap_get1(soap)) == 0xBF)
      { soap->mode &= ~SOAP_ENC_LATIN;
        c = soap_getchar(soap);
      }
      else
        c = (0x0F << 12) | (0xBB << 6) | (c & 0x3F); /* UTF-8 */
    }
    else if ((c == 0xFE && soap_get0(soap) == 0xFF)  /* UTF-16 BE */
          || (c == 0xFF && soap_get0(soap) == 0xFE)) /* UTF-16 LE */
      return soap->error = SOAP_UTF_ERROR;
    /* skip space */
    while (soap_blank(c))
      c = soap_getchar(soap);
  }
  if ((int)c == EOF)
    return soap->error = SOAP_CHK_EOF;
  soap_unget(soap, c);
#ifndef WITH_NOHTTP
  /* if not XML/MIME/DIME/ZLIB, assume HTTP method or status line */
  if (((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) && !(soap->mode & (SOAP_ENC_MIME | SOAP_ENC_DIME | SOAP_ENC_ZLIB | SOAP_ENC_XML)))
  { soap_mode m = soap->imode;
    soap->mode &= ~SOAP_IO;
    soap->error = soap->fparse(soap);
    if (soap->error && soap->error < SOAP_STOP)
    { soap->keep_alive = 0; /* force close later */
      return soap->error;
    }
    if (soap->error == SOAP_STOP)
    { if (soap->fform)
      { soap->error = soap->fform(soap);
        if (soap->error == SOAP_OK)
          soap->error = SOAP_STOP; /* prevents further processing */
      }
      return soap->error;
    }
    soap->mode = soap->imode; /* if imode is changed, effectuate */
    soap->imode = m; /* restore imode */
#ifdef WITH_ZLIB
    soap->mode &= ~SOAP_ENC_ZLIB;
#endif
    if ((soap->mode & SOAP_IO) == SOAP_IO_CHUNK)
    { soap->chunkbuflen = soap->buflen;
      soap->buflen = soap->bufidx;
      soap->chunksize = 0;
    }
    /* Note: fparse should not use soap_unget to push back last char */
#if 0
    if (soap->status > 200 && soap->length == 0 && !(soap->http_content && (!soap->keep_alive || soap->recv_timeout)) && (soap->imode & SOAP_IO) != SOAP_IO_CHUNK)
#endif
    if (soap->status && !soap->body)
      return soap->error = soap->status;
#ifdef WITH_ZLIB
    if (soap->zlib_in != SOAP_ZLIB_NONE)
    {
#ifdef WITH_GZIP
      if (soap->zlib_in != SOAP_ZLIB_DEFLATE)
      { c = soap_get1(soap);
        if (c == (int)EOF)
          return soap->error = SOAP_EOF;
        if (c == 0x1F)
        { if (soap_getgziphdr(soap))
            return soap->error;
          if (inflateInit2(soap->d_stream, -MAX_WBITS) != Z_OK)
            return soap->error = SOAP_ZLIB_ERROR;
          soap->z_crc = crc32(0L, NULL, 0);
          DBGLOG(TEST, SOAP_MESSAGE(fdebug, "gzip initialized\n"));
        }
        else
        { soap_revget1(soap);
          if (inflateInit(soap->d_stream) != Z_OK)
            return soap->error = SOAP_ZLIB_ERROR;
          soap->zlib_in = SOAP_ZLIB_DEFLATE;
        }
      }
      else
#endif
      if (inflateInit(soap->d_stream) != Z_OK)
        return soap->error = SOAP_ZLIB_ERROR;
      if (soap->z_dict)
      { if (inflateSetDictionary(soap->d_stream, (const Bytef*)soap->z_dict, soap->z_dict_len) != Z_OK)
          return soap->error = SOAP_ZLIB_ERROR;
      }
      soap->zlib_state = SOAP_ZLIB_INFLATE;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Inflate initialized\n"));
      soap->mode |= SOAP_ENC_ZLIB;
      if (!soap->z_buf)
        soap->z_buf = (char*)SOAP_MALLOC(soap, sizeof(soap->buf));
      soap_memcpy((void*)soap->z_buf, sizeof(soap->buf), (const void*)soap->buf, sizeof(soap->buf));
      soap->d_stream->next_in = (Byte*)(soap->z_buf + soap->bufidx);
      soap->d_stream->avail_in = (unsigned int)(soap->buflen - soap->bufidx);
      soap->z_buflen = soap->buflen;
      soap->buflen = soap->bufidx;
    }
#endif
#ifndef WITH_LEANER
    if (soap->fpreparerecv && (soap->mode & SOAP_IO) != SOAP_IO_CHUNK && soap->buflen > soap->bufidx)
    { int r;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Invoking fpreparerecv\n"));
      if ((r = soap->fpreparerecv(soap, soap->buf + soap->bufidx, soap->buflen - soap->bufidx)))
        return soap->error = r;
    }
#endif
    if (soap_get0(soap) == (int)EOF)
    { if (soap->status == 0)
        return soap->error = SOAP_NO_DATA; /* server side expects data */
      return soap->error = soap->status; /* client side received HTTP status code */
    }
    if (soap->error)
    { if (soap->error == SOAP_FORM && soap->fform)
      { soap->error = soap->fform(soap);
        if (soap->error == SOAP_OK)
          soap->error = SOAP_STOP; /* prevents further processing */
      }
      return soap->error;
    }
  }
#endif
#ifndef WITH_LEANER
  if (soap->mode & SOAP_ENC_MIME)
  { do /* skip preamble */
    { if ((int)(c = soap_getchar(soap)) == EOF)
        return soap->error = SOAP_CHK_EOF;
    } while (c != '-' || soap_get0(soap) != '-');
    soap_unget(soap, c);
    if (soap_getmimehdr(soap))
      return soap->error;
    if (soap->mime.start)
    { do
      { if (!soap->mime.last->id)
          break;
        if (!soap_match_cid(soap, soap->mime.start, soap->mime.last->id))
          break;
      } while (soap_get_mime_attachment(soap, NULL));
    }
    if (soap_get_header_attribute(soap, soap->mime.first->type, "application/dime"))
      soap->mode |= SOAP_ENC_DIME;
  }
  if (soap->mode & SOAP_ENC_DIME)
  { if (soap_getdimehdr(soap))
      return soap->error;
    if (soap->dime.flags & SOAP_DIME_CF)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Chunked DIME SOAP message\n"));
      soap->dime.chunksize = soap->dime.size;
      if (soap->buflen - soap->bufidx >= soap->dime.chunksize)
      { soap->dime.buflen = soap->buflen;
        soap->buflen = soap->bufidx + soap->dime.chunksize;
      }
      else
        soap->dime.chunksize -= soap->buflen - soap->bufidx;
    }
    soap->count = soap->buflen - soap->bufidx;
  }
#endif
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_envelope_begin_out(struct soap *soap)
{
#ifndef WITH_LEANER
  size_t n = 0;
  if ((soap->mode & SOAP_ENC_MIME) && soap->mime.boundary && soap->mime.start)
  { const char *s;
    if (strlen(soap->mime.boundary) + strlen(soap->mime.start) + 140 > sizeof(soap->tmpbuf))
      return soap->error = SOAP_EOM;
    if ((soap->mode & SOAP_ENC_DIME) && !(soap->mode & SOAP_ENC_MTOM))
      s = "application/dime";
    else if (soap->version == 2)
    { if (soap->mode & SOAP_ENC_MTOM)
        s = "application/xop+xml; charset=utf-8; type=\"application/soap+xml\"";
      else
        s = "application/soap+xml; charset=utf-8";
    }
    else if (soap->mode & SOAP_ENC_MTOM)
      s = "application/xop+xml; charset=utf-8; type=\"text/xml\"";
    else
      s = "text/xml; charset=utf-8";
    (SOAP_SNPRINTF_SAFE(soap->tmpbuf, sizeof(soap->tmpbuf)), "--%s\r\nContent-Type: %s\r\nContent-Transfer-Encoding: binary\r\nContent-ID: %s\r\n\r\n", soap->mime.boundary, s, soap->mime.start);
    n = strlen(soap->tmpbuf);
    if (soap_send_raw(soap, soap->tmpbuf, n))
      return soap->error;
  }
  if (soap->mode & SOAP_IO_LENGTH)
    soap->dime.size = soap->count;	/* DIME in MIME correction */
  if (!(soap->mode & SOAP_IO_LENGTH) && (soap->mode & SOAP_ENC_DIME))
  { if (soap_putdimehdr(soap))
      return soap->error;
  }
#endif
  if (soap->version == 0)
    return SOAP_OK;
  soap->part = SOAP_IN_ENVELOPE;
  return soap_element_begin_out(soap, "SOAP-ENV:Envelope", 0, NULL);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_envelope_end_out(struct soap *soap)
{ if (soap->version == 0)
    return SOAP_OK;
  if (soap_element_end_out(soap, "SOAP-ENV:Envelope")
   || soap_send_raw(soap, "\r\n", 2))	/* 2.8: always emit \r\n */
    return soap->error;
#ifndef WITH_LEANER
  if ((soap->mode & SOAP_IO_LENGTH) && (soap->mode & SOAP_ENC_DIME) && !(soap->mode & SOAP_ENC_MTOM))
  { soap->dime.size = soap->count - soap->dime.size;	/* DIME in MIME correction */
    (SOAP_SNPRINTF(soap->id, sizeof(soap->id), strlen(soap->dime_id_format) + 20), soap->dime_id_format, 0);
    soap->dime.id = soap->id;
    if (soap->local_namespaces)
    { if (soap->local_namespaces[0].out)
        soap->dime.type = (char*)soap->local_namespaces[0].out;
      else
        soap->dime.type = (char*)soap->local_namespaces[0].ns;
    }
    soap->dime.options = NULL;
    soap->dime.flags = SOAP_DIME_MB | SOAP_DIME_ABSURI;
    if (!soap->dime.first)
      soap->dime.flags |= SOAP_DIME_ME;
    soap->count += 12 + ((strlen(soap->dime.id)+3)&(~3)) + (soap->dime.type ? ((strlen(soap->dime.type)+3)&(~3)) : 0);
  }
  if ((soap->mode & SOAP_ENC_DIME) && !(soap->mode & SOAP_ENC_MTOM))
    return soap_send_raw(soap, SOAP_STR_PADDING, -(long)soap->dime.size&3);
#endif
  soap->part = SOAP_END_ENVELOPE;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifndef PALM_1
SOAP_FMAC1
char*
SOAP_FMAC2
soap_get_http_body(struct soap *soap, size_t *len)
{
#ifndef WITH_LEAN
  size_t l = 0, n = 0;
  char *s;
  if (len)
    *len = 0;
  /* get HTTP body length */
  if (!(soap->mode & SOAP_ENC_ZLIB) && (soap->mode & SOAP_IO) != SOAP_IO_CHUNK)
  { n = soap->length;
    if (!n)
      return NULL;
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Parsing HTTP body (mode=0x%x,len=%lu)\n", soap->mode, (unsigned long)n));
#ifdef WITH_FAST
  soap->labidx = 0;			/* use look-aside buffer */
#else
  if (soap_new_block(soap) == NULL)
    return NULL;
#endif
  for (;;)
  {
#ifdef WITH_FAST
    size_t i, k;
    if (soap_append_lab(soap, NULL, 0))	/* allocate more space in look-aside buffer if necessary */
      return NULL;
    s = soap->labbuf + soap->labidx;	/* space to populate */
    k = soap->lablen - soap->labidx;	/* number of bytes available */
    soap->labidx = soap->lablen;	/* claim this space */
#else
    size_t i, k = SOAP_BLKLEN;
    if (!(s = (char*)soap_push_block(soap, NULL, k)))
      return NULL;
#endif
    for (i = 0; i < k; i++)
    { soap_wchar c;
      l++;
      if (n > 0 && l > n)
        goto end;
      c = soap_get1(soap);
      if ((int)c == EOF)
        goto end;
      *s++ = (char)(c & 0xFF);
    }
  }
end:
  *s = '\0';
  if (len)
    *len = l - 1; /* len excludes terminating \0 */
#ifdef WITH_FAST
  if ((s = (char*)soap_malloc(soap, l)))
    soap_memcpy((void*)s, l, (const void*)soap->labbuf, l);
#else
  soap_size_block(soap, NULL, i+1);
  s = soap_save_block(soap, NULL, NULL, 0);
#endif
  return s;
#else
  if (len)
    *len = 0;
  return NULL;
#endif
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_envelope_begin_in(struct soap *soap)
{ soap->part = SOAP_IN_ENVELOPE;
  if (soap_element_begin_in(soap, "SOAP-ENV:Envelope", 0, NULL))
  { if (soap->error == SOAP_TAG_MISMATCH)
    { if (!soap_element_begin_in(soap, "Envelope", 0, NULL))
        soap->error = SOAP_VERSIONMISMATCH;
      else if (soap->status == 0 || (soap->status >= 200 && soap->status <= 299))
        return SOAP_OK; /* allow non-SOAP (REST) XML content to be captured */
      soap->error = soap->status;
    }
    else if (soap->status)
      soap->error = soap->status;
    return soap->error;
  }
  soap_get_version(soap);
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_envelope_end_in(struct soap *soap)
{ if (soap->version == 0)
    return SOAP_OK;
  soap->part = SOAP_END_ENVELOPE;
  return soap_element_end_in(soap, "SOAP-ENV:Envelope");
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_body_begin_out(struct soap *soap)
{ if (soap->version == 1)
    soap->encoding = 1;
#ifndef WITH_LEAN
  if ((soap->mode & SOAP_SEC_WSUID) && soap_set_attr(soap, "wsu:Id", "Body", 1))
    return soap->error;
#endif
  if (soap->version == 0)
    return SOAP_OK;
  soap->part = SOAP_IN_BODY;
  return soap_element_begin_out(soap, "SOAP-ENV:Body", 0, NULL);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_body_end_out(struct soap *soap)
{ if (soap->version == 0)
    return SOAP_OK;
  if (soap_element_end_out(soap, "SOAP-ENV:Body"))
    return soap->error;
  soap->part = SOAP_END_BODY;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_body_begin_in(struct soap *soap)
{ if (soap->version == 0)
    return SOAP_OK;
  soap->part = SOAP_IN_BODY;
  if (soap_element_begin_in(soap, "SOAP-ENV:Body", 0, NULL))
    return soap->error;
  if (!soap->body)
    soap->part = SOAP_NO_BODY;
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_body_end_in(struct soap *soap)
{ if (soap->version == 0)
    return SOAP_OK;
  if (soap->part == SOAP_NO_BODY)
    return soap->error = SOAP_OK;
  soap->part = SOAP_END_BODY;
  return soap_element_end_in(soap, "SOAP-ENV:Body");
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_recv_header(struct soap *soap)
{ if (soap_getheader(soap) && soap->error == SOAP_TAG_MISMATCH)
    soap->error = SOAP_OK;
  if (soap->error == SOAP_OK && soap->fheader)
    soap->error = soap->fheader(soap);
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_endpoint(struct soap *soap, const char *endpoint)
{ const char *s;
  size_t i, n;
  soap->endpoint[0] = '\0';
  soap->host[0] = '\0';
  soap->path[0] = '/';
  soap->path[1] = '\0';
  soap->port = 80;
  if (!endpoint || !*endpoint)
    return;
#ifdef WITH_OPENSSL
  if (!soap_tag_cmp(endpoint, "https:*"))
    soap->port = 443;
#endif
  soap_strcpy(soap->endpoint, sizeof(soap->endpoint), endpoint);
  s = strchr(endpoint, ':');
  if (s && s[1] == '/' && s[2] == '/')
    s += 3;
  else
    s = endpoint;
  n = strlen(s);
  if (n >= sizeof(soap->host))
    n = sizeof(soap->host) - 1;
#ifdef WITH_IPV6
  if (s[0] == '[')
  { s++;
    for (i = 0; i < n; i++)
    { if (s[i] == ']')
      { s++;
        --n;
        break;
      }
      soap->host[i] = s[i];
    }
  }
  else
  { for (i = 0; i < n; i++)
    { soap->host[i] = s[i];
      if (s[i] == '/' || s[i] == ':')
        break;
    }
  }
#else
  for (i = 0; i < n; i++)
  { soap->host[i] = s[i];
    if (s[i] == '/' || s[i] == ':')
      break;
  }
#endif
  soap->host[i] = '\0';
  if (s[i] == ':')
  { soap->port = (int)soap_strtol(s + i + 1, NULL, 10);
    for (i++; i < n; i++)
      if (s[i] == '/')
        break;
  }
  if (i < n && s[i])
    soap_strcpy(soap->path, sizeof(soap->path), s + i);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_connect(struct soap *soap, const char *endpoint, const char *action)
{ return soap_connect_command(soap, SOAP_POST, endpoint, action);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_connect_command(struct soap *soap, int http_command, const char *endpoints, const char *action)
{ char *endpoint;
  const char *s;
  if (endpoints && (s = strchr(endpoints, ' ')))
  { size_t l = strlen(endpoints);
    endpoint = (char*)SOAP_MALLOC(soap, l + 1);
    for (;;)
    { soap_strncpy(endpoint, l + 1, endpoints, s - endpoints);
      endpoint[s - endpoints] = '\0';
      if (soap_try_connect_command(soap, http_command, endpoint, action) != SOAP_TCP_ERROR)
        break;
      if (!*s)
        break;
      soap->error = SOAP_OK;
      while (*s == ' ')
        s++;
      endpoints = s;
      s = strchr(endpoints, ' ');
      if (!s)
        s = endpoints + strlen(endpoints);
    }
    SOAP_FREE(soap, endpoint);
  }
  else
    soap_try_connect_command(soap, http_command, endpoints, action);
  return soap->error;
}
#endif

/******************************************************************************/
#ifndef PALM_1
static int
soap_try_connect_command(struct soap *soap, int http_command, const char *endpoint, const char *action)
{ char host[sizeof(soap->host)];
  int port;
  size_t count;
  soap->error = SOAP_OK;
  soap_strcpy(host, sizeof(soap->host), soap->host); /* save previous host name: if != then reconnect */
  port = soap->port; /* save previous port to compare */
  soap->status = http_command;
  soap_set_endpoint(soap, endpoint);
  soap->action = soap_strdup(soap, action);
#ifndef WITH_LEANER
  if (soap->fconnect)
  { if ((soap->error = soap->fconnect(soap, endpoint, soap->host, soap->port)))
      return soap->error;
  }
  else
#endif
  if (soap->fopen && *soap->host)
  { if (!soap->keep_alive || !soap_valid_socket(soap->socket) || strcmp(soap->host, host) || soap->port != port || !soap->fpoll || soap->fpoll(soap))
    { soap->error = SOAP_OK;
#ifndef WITH_LEAN
      if (!strncmp(endpoint, "soap.udp:", 9))
        soap->omode |= SOAP_IO_UDP;
      else
#endif
      { soap->keep_alive = 0; /* to force close */
        soap->omode &= ~SOAP_IO_UDP; /* to force close */
      }
      soap_closesock(soap);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Connect/reconnect to '%s' host='%s' path='%s' port=%d\n", endpoint?endpoint:"(null)", soap->host, soap->path, soap->port));
      if (!soap->keep_alive || !soap_valid_socket(soap->socket))
      { soap->socket = soap->fopen(soap, endpoint, soap->host, soap->port);
        if (soap->error)
          return soap->error;
        soap->keep_alive = ((soap->omode & SOAP_IO_KEEPALIVE) != 0);
      }
    }
  }
#ifdef WITH_NTLM
  if (soap_ntlm_handshake(soap, SOAP_GET, endpoint, soap->host, soap->port))
    return soap->error;
#endif
  count = soap_count_attachments(soap);
  if (soap_begin_send(soap))
    return soap->error;
  if (http_command == SOAP_GET)
  { soap->mode &= ~SOAP_IO;
    soap->mode |= SOAP_IO_BUFFER;
  }
#ifndef WITH_NOHTTP
  if ((soap->mode & SOAP_IO) != SOAP_IO_STORE && !(soap->mode & SOAP_ENC_XML) && endpoint)
  { unsigned int k = soap->mode;
    soap->mode &= ~(SOAP_IO | SOAP_ENC_ZLIB);
    if ((k & SOAP_IO) != SOAP_IO_FLUSH)
      soap->mode |= SOAP_IO_BUFFER;
    if ((soap->error = soap->fpost(soap, endpoint, soap->host, soap->port, soap->path, action, count)))
      return soap->error;
#ifndef WITH_LEANER
    if ((k & SOAP_IO) == SOAP_IO_CHUNK)
    { if (soap_flush(soap))
        return soap->error;
    }
#endif
    soap->mode = k;
  }
  if (http_command == SOAP_GET || http_command == SOAP_DEL)
    return soap_end_send_flush(soap);
#endif
  return SOAP_OK;
}
#endif

/******************************************************************************/
#ifdef WITH_NTLM
#ifndef PALM_1
static int
soap_ntlm_handshake(struct soap *soap, int command, const char *endpoint, const char *host, int port)
{ /* requires libntlm from http://www.nongnu.org/libntlm/ */
  const char *userid = (soap->proxy_userid ? soap->proxy_userid : soap->userid);
  const char *passwd = (soap->proxy_passwd ? soap->proxy_passwd : soap->passwd);
  struct SOAP_ENV__Header *oldheader;
  if (soap->ntlm_challenge && userid && passwd && soap->authrealm)
  { tSmbNtlmAuthRequest req;  
    tSmbNtlmAuthResponse res;
    tSmbNtlmAuthChallenge ch;
    short k = soap->keep_alive;
    size_t l = soap->length;
    size_t c = soap->count;
    soap_mode m = soap->mode, o = soap->omode;
    int s = soap->status;
    char *a = soap->action;
    short v = soap->version;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "NTLM '%s'\n", soap->ntlm_challenge));
    if (!*soap->ntlm_challenge)
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "NTLM S->C Type 1: received NTLM authentication challenge from server\n"));
      /* S -> C   401 Unauthorized
                  WWW-Authenticate: NTLM
      */
      buildSmbNtlmAuthRequest(&req, userid, soap->authrealm);
      soap->ntlm_challenge = soap_s2base64(soap, (unsigned char*)(void*)&req, NULL, SmbLength(&req));
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "NTLM C->S Type 2: sending NTLM authorization to server\nAuthorization: NTLM %s\n", soap->ntlm_challenge));
      /* C -> S   GET ...
                  Authorization: NTLM TlRMTVNTUAABAAAAA7IAAAoACgApAAAACQAJACAAAABMSUdIVENJVFlVUlNBLU1JTk9S
      */
      soap->omode = SOAP_IO_BUFFER;
      if (soap_begin_send(soap))
        return soap->error;
      soap->keep_alive = 1;
      soap->status = command;
      if (soap->fpost(soap, endpoint, host, port, soap->path, soap->action, 0)
       || soap_end_send_flush(soap))
        return soap->error;
      soap->mode = m;
      soap->keep_alive = k;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "NTLM S->C Type 2: waiting on server NTLM response\n"));
      oldheader = soap->header;
      if (soap_begin_recv(soap))
        if (soap->error == SOAP_EOF)
          return soap->error;
      soap_end_recv(soap);
      soap->header = oldheader;
      soap->length = l;
      if (soap->status != 401 && soap->status != 407)
        return soap->error = SOAP_NTLM_ERROR;
      soap->error = SOAP_OK;
    }
    /* S -> C   401 Unauthorized
                WWW-Authenticate: NTLM TlRMTVNTUAACAAAAAAAAACgAAAABggAAU3J2Tm9uY2UAAAAAAAAAAA==
    */
    soap_base642s(soap, soap->ntlm_challenge, (char*)&ch, sizeof(tSmbNtlmAuthChallenge), NULL);
    buildSmbNtlmAuthResponse(&ch, &res, userid, passwd);
    soap->ntlm_challenge = soap_s2base64(soap, (unsigned char*)(void*)&res, NULL, SmbLength(&res));
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "NTLM C->S Type 3: sending NTLM authorization to server\nAuthorization: NTLM %s\n", soap->ntlm_challenge));
    /* C -> S   GET ...
                Authorization: NTLM TlRMTVNTUAADAAAAGAAYAHIAAAAYABgAigAAABQAFABAAAAADAAMAFQAAAASABIAYAAAAAAAAACiAAAAAYIAAFUAUgBTAEEALQBNAEkATgBPAFIAWgBhAHAAaABvAGQATABJAEcASABUAEMASQBUAFkArYfKbe/jRoW5xDxHeoxC1gBmfWiS5+iX4OAN4xBKG/IFPwfH3agtPEia6YnhsADT
    */
    soap->userid = NULL;
    soap->passwd = NULL;
    soap->proxy_userid = NULL;
    soap->proxy_passwd = NULL;
    soap->keep_alive = k;
    soap->length = l;
    soap->count = c;
    soap->mode = m;
    soap->omode = o;
    soap->status = s;
    soap->action = a;
    soap->version = v;
  }
  return SOAP_OK;
}
#endif
#endif

/******************************************************************************/
#if !defined(WITH_LEAN) || defined(WITH_NTLM)
SOAP_FMAC1
char*
SOAP_FMAC2
soap_s2base64(struct soap *soap, const unsigned char *s, char *t, int n)
{ int i;
  unsigned long m;
  char *p;
  if (!t)
    t = (char*)soap_malloc(soap, (n + 2) / 3 * 4 + 1);
  if (!t)
    return NULL;
  p = t;
  t[0] = '\0';
  if (!s)
    return p;
  for (; n > 2; n -= 3, s += 3)
  { m = s[0];
    m = (m << 8) | s[1];
    m = (m << 8) | s[2];
    for (i = 4; i > 0; m >>= 6)
      t[--i] = soap_base64o[m & 0x3F];
    t += 4;
  }
  t[0] = '\0';
  if (n > 0) /* 0 < n <= 2 implies that t[0..4] is allocated (base64 scaling formula) */
  { m = 0;
    for (i = 0; i < n; i++)
      m = (m << 8) | *s++;
    for (; i < 3; i++)
      m <<= 8;
    for (i = 4; i > 0; m >>= 6)
      t[--i] = soap_base64o[m & 0x3F];
    for (i = 3; i > n; i--)
      t[i] = '=';
    t[4] = '\0';
  }
  return p;
}
#endif

/******************************************************************************/
#if !defined(WITH_LEAN) || defined(WITH_NTLM)
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_base642s(struct soap *soap, const char *s, char *t, size_t l, int *n)
{ size_t i, j;
  soap_wchar c;
  unsigned long m;
  const char *p;
  if (!s || !*s)
  { if (n)
      *n = 0;
    if (soap->error)
      return NULL;
    return SOAP_NON_NULL;
  }
  if (!t)
  { l = (strlen(s) + 3) / 4 * 3 + 1;	/* space for raw binary and \0 */
    t = (char*)soap_malloc(soap, l);
  }
  if (!t)
    return NULL;
  p = t;
  if (n)
    *n = 0;
  for (i = 0; ; i += 3, l -= 3)
  { m = 0;
    j = 0;
    while (j < 4)
    { c = *s++;
      if (c == '=' || !c)
      { if (l >= j - 1)
        { switch (j)
          { case 2:
              *t++ = (char)((m >> 4) & 0xFF);
              i++;
              l--;
              break;
            case 3:
              *t++ = (char)((m >> 10) & 0xFF);
              *t++ = (char)((m >> 2) & 0xFF);
              i += 2;
              l -= 2;
          }
        }
        if (n)
          *n = (int)i;
        if (l)
          *t = '\0';
        return p;
      }
      c -= '+';
      if (c >= 0 && c <= 79)
      { int b = soap_base64i[c];
        if (b >= 64)
        { soap->error = SOAP_TYPE;
          return NULL;
        }
        m = (m << 6) + b;
        j++;
      }
      else if (!soap_blank(c + '+'))
      { soap->error = SOAP_TYPE;
        return NULL;
      }
    }
    if (l < 3)
    { if (n)
        *n = (int)i;
      if (l)
        *t = '\0';
      return p;
    }
    *t++ = (char)((m >> 16) & 0xFF);
    *t++ = (char)((m >> 8) & 0xFF);
    *t++ = (char)(m & 0xFF);
  }
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
char*
SOAP_FMAC2
soap_s2hex(struct soap *soap, const unsigned char *s, char *t, int n)
{ char *p;
  if (!t)
    t = (char*)soap_malloc(soap, 2 * n + 1);
  if (!t)
    return NULL;
  p = t;
  t[0] = '\0';
  if (s)
  { for (; n > 0; n--)
    { int m = *s++;
      *t++ = (char)((m >> 4) + (m > 159 ? 'a' - 10 : '0'));
      m &= 0x0F;
      *t++ = (char)(m + (m > 9 ? 'a' - 10 : '0'));
    }
  }
  *t++ = '\0';
  return p;
}
#endif

/******************************************************************************/
#ifndef WITH_LEAN
SOAP_FMAC1
const char*
SOAP_FMAC2
soap_hex2s(struct soap *soap, const char *s, char *t, size_t l, int *n)
{ const char *p;
  if (!s || !*s)
  { if (n)
      *n = 0;
    if (soap->error)
      return NULL;
    return SOAP_NON_NULL;
  }
  if (!t)
  { l = strlen(s) / 2 + 1;	/* make sure enough space for \0 */
    t = (char*)soap_malloc(soap, l);
  }
  if (!t)
    return NULL;
  p = t;
  while (l)
  { int d1, d2;
    d1 = *s++;
    if (!d1)
      break;
    d2 = *s++;
    if (!d2)
      break;
    *t++ = (char)(((d1 >= 'A' ? (d1 & 0x7) + 9 : d1 - '0') << 4) + (d2 >= 'A' ? (d2 & 0x7) + 9 : d2 - '0'));
    l--;
  }
  if (n)
    *n = (int)(t - p);
  if (l)
    *t = '\0';
  return p;
}
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_puthttphdr(struct soap *soap, int status, size_t count)
{ if (soap->status != SOAP_GET && soap->status != SOAP_DEL && soap->status != SOAP_CONNECT)
  { const char *s = "text/xml; charset=utf-8";
    int err = SOAP_OK;
#ifndef WITH_LEANER
    const char *r = NULL;
    size_t n;
#endif
    if ((status == SOAP_FILE || soap->status == SOAP_PUT || soap->status == SOAP_POST_FILE) && soap->http_content && !strchr(s, 10) && !strchr(s, 13))
      s = soap->http_content;
    else if (status == SOAP_HTML)
      s = "text/html; charset=utf-8";
    else if (count || ((soap->omode & SOAP_IO) == SOAP_IO_CHUNK))
    { if (soap->version == 2)
        s = "application/soap+xml; charset=utf-8";
    }
#ifndef WITH_LEANER
    if (soap->mode & (SOAP_ENC_DIME | SOAP_ENC_MTOM))
    { if (soap->mode & SOAP_ENC_MTOM)
      { if (soap->version == 2)
          r = "application/soap+xml";
        else
          r = "text/xml";
        s = "application/xop+xml";
      }
      else
        s = "application/dime";
    }
    if ((soap->mode & SOAP_ENC_MIME) && soap->mime.boundary)
    { const char *t;
      size_t l;
      n = strlen(soap->mime.boundary);
      (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), n + 53), "multipart/related; charset=utf-8; boundary=\"%s\"; type=\"", soap->mime.boundary);
      t = strchr(s, ';');
      if (t)
	n = t - s;
      else
	n = strlen(s);
      l = strlen(soap->tmpbuf);
      if (sizeof(soap->tmpbuf) - l > n)
	soap_strncpy(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, s, n);
      if (soap->mime.start)
      { l = strlen(soap->tmpbuf);
	n = strlen(soap->mime.start);
        (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, n + 10), "\"; start=\"%s", soap->mime.start);
      }
      if (r)
      { l = strlen(soap->tmpbuf);
	n = strlen(r);
        (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, n + 15), "\"; start-info=\"%s", r);
      }
      l = strlen(soap->tmpbuf);
      if (sizeof(soap->tmpbuf) - l > 1)
	soap_strncpy(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, "\"", 1);
    }
    else
      soap_strcpy(soap->tmpbuf, sizeof(soap->tmpbuf), s);
    if (status == SOAP_OK && soap->version == 2 && soap->action)
    { size_t l = strlen(soap->tmpbuf);
      (SOAP_SNPRINTF(soap->tmpbuf + l, sizeof(soap->tmpbuf) - l, n + 11), "; action=\"%s\"", soap->action);
    }
#endif
    if ((err = soap->fposthdr(soap, "Content-Type", soap->tmpbuf)))
      return err;
#ifdef WITH_ZLIB
    if ((soap->omode & SOAP_ENC_ZLIB))
    {
#ifdef WITH_GZIP
      err = soap->fposthdr(soap, "Content-Encoding", soap->zlib_out == SOAP_ZLIB_DEFLATE ? "deflate" : "gzip");
#else
      err = soap->fposthdr(soap, "Content-Encoding", "deflate");
#endif
      if (err)
        return err;
    }
#endif
#ifndef WITH_LEANER
    if ((soap->omode & SOAP_IO) == SOAP_IO_CHUNK)
      err = soap->fposthdr(soap, "Transfer-Encoding", "chunked");
    else
#endif
    { (SOAP_SNPRINTF(soap->tmpbuf, sizeof(soap->tmpbuf), 20), SOAP_ULONG_FORMAT, (ULONG64)count);
      err = soap->fposthdr(soap, "Content-Length", soap->tmpbuf);
    }
    if (err)
      return err;
  }
  return soap->fposthdr(soap, "Connection", soap->keep_alive ? "keep-alive" : "close");
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
static const char*
soap_set_validation_fault(struct soap *soap, const char *s, const char *t)
{ if (!t)
    t = SOAP_STR_EOS;
  if (*soap->tag)
    (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(s) + strlen(t) + strlen(soap->tag) + 47), "Validation constraint violation: %s%s in element '%s'", s, t, soap->tag);
  else
    (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(s) + strlen(t) + 33), "Validation constraint violation: %s%s", s, t);
  return soap->msgbuf;
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
void
SOAP_FMAC2
soap_set_fault(struct soap *soap)
{ const char **c = soap_faultcode(soap);
  const char **s = soap_faultstring(soap);
  if (soap->fseterror)
    soap->fseterror(soap, c, s);
  if (!*c)
  { if (soap->version == 2)
      *c = "SOAP-ENV:Sender";
    else if (soap->version == 1)
      *c = "SOAP-ENV:Client";
    else
      *c = "at source";
  }
  if (*s)
    return;
  switch (soap->error)
  {
#ifndef WITH_LEAN
    case SOAP_CLI_FAULT:
      *s = "Client fault";
      break;
    case SOAP_SVR_FAULT:
      *s = "Server fault";
      break;
    case SOAP_TAG_MISMATCH:
      *s = soap_set_validation_fault(soap, "tag name or namespace mismatch", NULL);
      break;
    case SOAP_TYPE:
      if (*soap->type)
	*s = soap_set_validation_fault(soap, "type mismatch ", soap->type);
      else
	*s = soap_set_validation_fault(soap, "invalid value", NULL);
      break;
    case SOAP_SYNTAX_ERROR:
      *s = soap_set_validation_fault(soap, "syntax error", NULL);
      break;
    case SOAP_NO_TAG:
      if (soap->version == 0 && soap->level == 0)
	*s = soap_set_validation_fault(soap, "missing root element", NULL);
      else if (soap->version != 0 && soap->level < 3)
	*s = soap_set_validation_fault(soap, "missing SOAP message", NULL);
      else
	*s = soap_set_validation_fault(soap, "missing element", NULL);
      break;
    case SOAP_MUSTUNDERSTAND:
      *c = "SOAP-ENV:MustUnderstand";
      (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(soap->tag) + 65), "The data in element '%s' must be understood but cannot be processed", soap->tag);
      *s = soap->msgbuf;
      break;
    case SOAP_VERSIONMISMATCH:
      *c = "SOAP-ENV:VersionMismatch";
      *s = "Invalid SOAP message or SOAP version mismatch";
      break;
    case SOAP_DATAENCODINGUNKNOWN:
      *c = "SOAP-ENV:DataEncodingUnknown";
      *s = "Unsupported SOAP data encoding";
      break;
    case SOAP_NAMESPACE:
      *s = soap_set_validation_fault(soap, "namespace error", NULL);
      break;
    case SOAP_USER_ERROR:
      *s = "User data access error";
      break;
    case SOAP_FATAL_ERROR:
      *s = "A fatal error has occurred";
      break;
    case SOAP_NO_METHOD:
      (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(soap->tag) + 66), "Method '%s' not implemented: method name or namespace not recognized", soap->tag);
      *s = soap->msgbuf;
      break;
    case SOAP_NO_DATA:
      *s = "Data required for operation";
      break;
    case SOAP_GET_METHOD:
      *s = "HTTP GET method not implemented";
      break;
    case SOAP_PUT_METHOD:
      *s = "HTTP PUT method not implemented";
      break;
    case SOAP_HTTP_METHOD:
      *s = "HTTP method not implemented";
      break;
    case SOAP_EOM:
      *s = "Out of memory";
      break;
    case SOAP_MOE:
      *s = "Memory overflow or memory corruption error";
      break;
    case SOAP_HDR:
      *s = "Header line too long";
      break;
    case SOAP_IOB:
      *s = "Array index out of bounds";
      break;
    case SOAP_NULL:
      *s = soap_set_validation_fault(soap, "nil not allowed", NULL);
      break;
    case SOAP_DUPLICATE_ID:
      *s = soap_set_validation_fault(soap, "multiple elements (use the SOAP_XML_TREE flag) with duplicate id ", soap->id);
      if (soap->version == 2)
        *soap_faultsubcode(soap) = "SOAP-ENC:DuplicateID";
      break;
    case SOAP_MISSING_ID:
      *s = soap_set_validation_fault(soap, "missing id for ref ", soap->id);
      if (soap->version == 2)
        *soap_faultsubcode(soap) = "SOAP-ENC:MissingID";
      break;
    case SOAP_HREF:
      *s = soap_set_validation_fault(soap, "incompatible object type id-ref ", soap->id);
      break;
    case SOAP_FAULT:
      break;
#ifndef WITH_NOIO
    case SOAP_UDP_ERROR:
      *s = "Message too large for UDP packet";
      break;
    case SOAP_TCP_ERROR:
      *s = tcp_error(soap);
      break;
#endif
    case SOAP_HTTP_ERROR:
      *s = "An HTTP processing error occurred";
      break;
    case SOAP_NTLM_ERROR:
      *s = "An HTTP NTLM authentication error occurred";
      break;
    case SOAP_SSL_ERROR:
#ifdef WITH_OPENSSL
      *s = "SSL/TLS error";
#else
      *s = "OpenSSL not installed: recompile with -DWITH_OPENSSL";
#endif
      break;
    case SOAP_PLUGIN_ERROR:
      *s = "Plugin registry error";
      break;
    case SOAP_DIME_ERROR:
      *s = "DIME format error or max DIME size exceeds SOAP_MAXDIMESIZE";
      break;
    case SOAP_DIME_HREF:
      *s = "DIME href to missing attachment";
      break;
    case SOAP_DIME_MISMATCH:
      *s = "DIME version/transmission error";
      break;
    case SOAP_DIME_END:
      *s = "End of DIME error";
      break;
    case SOAP_MIME_ERROR:
      *s = "MIME format error";
      break;
    case SOAP_MIME_HREF:
      *s = "MIME href to missing attachment";
      break;
    case SOAP_MIME_END:
      *s = "End of MIME error";
      break;
    case SOAP_ZLIB_ERROR:
#ifdef WITH_ZLIB
      (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), (soap->d_stream && soap->d_stream->msg ? strlen(soap->d_stream->msg) : 0) + 19), "Zlib/gzip error: '%s'", soap->d_stream && soap->d_stream->msg ? soap->d_stream->msg : SOAP_STR_EOS);
      *s = soap->msgbuf;
#else
      *s = "Zlib/gzip not installed for (de)compression: recompile with -DWITH_GZIP";
#endif
      break;
    case SOAP_REQUIRED:
      *s = soap_set_validation_fault(soap, "missing required attribute", NULL);
      break;
    case SOAP_PROHIBITED:
      *s = soap_set_validation_fault(soap, "prohibited attribute present", NULL);
      break;
    case SOAP_OCCURS:
      *s = soap_set_validation_fault(soap, "occurrence violation", NULL);
      break;
    case SOAP_LENGTH:
      *s = soap_set_validation_fault(soap, "content range or length violation", NULL);
      break;
    case SOAP_FD_EXCEEDED:
      *s = "Maximum number of open connections was reached (no define HAVE_POLL): increase FD_SETSIZE";
      break;
    case SOAP_UTF_ERROR:
      *s = "UTF content encoding error";
      break;
    case SOAP_STOP:
      *s = "Stopped: no response sent or received (informative)";
      break;
#endif
    case SOAP_EOF:
#ifndef WITH_NOIO
      *s = soap_strerror(soap); /* *s = soap->msgbuf */
#ifndef WITH_LEAN
      if (strlen(soap->msgbuf) + 25 < sizeof(soap->msgbuf))
      { soap_memmove((void*)(soap->msgbuf + 25), sizeof(soap->tmpbuf) - 25, (const void*)soap->msgbuf, strlen(soap->msgbuf) + 1);
        soap_memcpy((void*)soap->msgbuf, sizeof(soap->msgbuf), (const void*)"End of file or no input: ", 25);
      }
#endif
      break;
#else
      *s = "End of file or no input";
      break;
#endif
    default:
#ifndef WITH_NOHTTP
#ifndef WITH_LEAN
      if (soap->error >= 200 && soap->error < 600)
      { const char *t = http_error(soap, soap->error);
	(SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), strlen(t) + 54), "Error %d: HTTP %d %s", soap->error, soap->error, t);
        *s = soap->msgbuf;
      }
      else
#endif
#endif
      { (SOAP_SNPRINTF(soap->msgbuf, sizeof(soap->msgbuf), 26), "Error %d", soap->error);
        *s = soap->msgbuf;
      }
    }
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_send_fault(struct soap *soap)
{ int status = soap->error;
  if (status == SOAP_OK || status == SOAP_STOP)
    return soap_closesock(soap);
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Sending back fault struct for error code %d\n", soap->error));
  soap->keep_alive = 0; /* to terminate connection */
  soap_set_fault(soap);
  if (soap->error < 200 && soap->error != SOAP_FAULT)
    soap->header = NULL;
  if (status != SOAP_EOF || (!soap->recv_timeout && !soap->send_timeout))
  { int r = 1;
#ifndef WITH_NOIO
    if (soap->fpoll && soap->fpoll(soap))
      r = 0;
#ifndef WITH_LEAN
    else if (soap_valid_socket(soap->socket))
    { r = tcp_select(soap, soap->socket, SOAP_TCP_SELECT_RCV | SOAP_TCP_SELECT_SND, 0);
      if (r > 0)
      { int t;
        if (!(r & SOAP_TCP_SELECT_SND)
         || ((r & SOAP_TCP_SELECT_RCV)
          && recv(soap->socket, (char*)&t, 1, MSG_PEEK) < 0))
          r = 0;
      }
    }
#endif
#endif
    if (r > 0)
    { soap->error = SOAP_OK;
      soap->encodingStyle = NULL; /* no encodingStyle in Faults */
      soap_serializeheader(soap);
      soap_serializefault(soap);
      soap_begin_count(soap);
      if (soap->mode & SOAP_IO_LENGTH)
      { soap_envelope_begin_out(soap);
        soap_putheader(soap);
        soap_body_begin_out(soap);
        soap_putfault(soap);
        soap_body_end_out(soap);
        soap_envelope_end_out(soap);
      }
      soap_end_count(soap);
      if (soap_response(soap, status)
       || soap_envelope_begin_out(soap)
       || soap_putheader(soap)
       || soap_body_begin_out(soap)
       || soap_putfault(soap)
       || soap_body_end_out(soap)
       || soap_envelope_end_out(soap))
        return soap_closesock(soap);
      soap_end_send(soap);
    }
  }
  soap->error = status;
  return soap_closesock(soap);
}
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_recv_fault(struct soap *soap, int check)
{ int status = soap->status;
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Check (%d) if receiving SOAP Fault (status = %d)\n", check, status));
  if (!check)
  { /* try getfault when no tag or tag mismatched at level 2, otherwise ret */
    if (soap->error != SOAP_NO_TAG
     && (soap->error != SOAP_TAG_MISMATCH || soap->level != 2))
      return soap->error;
  }
  else if (soap->version == 0) /* check == 1 but no SOAP: do not parse SOAP Fault */
  {
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Not a SOAP protocol\n"));
    return SOAP_OK;
  }
  soap->error = SOAP_OK;
  if (soap_getfault(soap))
  { /* check flag set: check if SOAP Fault is present, if not just return */
    if (check && soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
      return soap->error = SOAP_OK;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Error: soap_get_soapfault() failed at level %u tag '%s'\n", soap->level, soap->tag));
    *soap_faultcode(soap) = (soap->version == 2 ? "SOAP-ENV:Sender" : "SOAP-ENV:Client");
    if (status)
      soap->error = status;
    else
      soap->error = status = SOAP_NO_DATA;
    soap_set_fault(soap);
  }
  else
  { const char *s = *soap_faultcode(soap);
    if (!soap_match_tag(soap, s, "SOAP-ENV:Server") || !soap_match_tag(soap, s, "SOAP-ENV:Receiver"))
      status = SOAP_SVR_FAULT;
    else if (!soap_match_tag(soap, s, "SOAP-ENV:Client") || !soap_match_tag(soap, s, "SOAP-ENV:Sender"))
      status = SOAP_CLI_FAULT;
    else if (!soap_match_tag(soap, s, "SOAP-ENV:MustUnderstand"))
      status = SOAP_MUSTUNDERSTAND;
    else if (!soap_match_tag(soap, s, "SOAP-ENV:VersionMismatch"))
      status = SOAP_VERSIONMISMATCH;
    else
    { DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Received SOAP Fault code %s\n", s));
      status = SOAP_FAULT;
    }
    if (!soap_body_end_in(soap))
      soap_envelope_end_in(soap);
  }
  soap_end_recv(soap);
  soap->error = status;
  return soap_closesock(soap);
}
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_send_empty_response(struct soap *soap, int httpstatuscode)
{ soap_mode m = soap->omode;
  if (!(m & SOAP_IO_UDP))
  { soap->count = 0;
    if ((m & SOAP_IO) == SOAP_IO_CHUNK)
      soap->omode = (m & ~SOAP_IO) | SOAP_IO_BUFFER;
    soap_response(soap, httpstatuscode);
    soap_end_send(soap); /* force end of sends */
    soap->error = SOAP_STOP; /* stops the server (from returning another response */
    soap->omode = m;
  }
  return soap_closesock(soap);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOHTTP
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_recv_empty_response(struct soap *soap)
{ if (!(soap->omode & SOAP_IO_UDP))
  { if (!soap_begin_recv(soap))
    { if (soap->body)
      { if ((soap->status != 400 && soap->status != 500)
         || soap_envelope_begin_in(soap)
         || soap_recv_header(soap)
         || soap_body_begin_in(soap))
        {
#ifndef WITH_LEAN
          const char *s = soap_get_http_body(soap, NULL);
#endif
          soap_end_recv(soap);
#ifndef WITH_LEAN
          if (s)
            soap_set_receiver_error(soap, "HTTP Error", s, soap->status);
#endif
        }
        else
          return soap_recv_fault(soap, 1);
      }
      else
        soap_end_recv(soap);
    }
    else if (soap->error == SOAP_NO_DATA || soap->error == 200 || soap->error == 202)
      soap->error = SOAP_OK;
  }
  return soap_closesock(soap);
}
#endif
#endif

/******************************************************************************/
#ifndef WITH_NOIO
#ifndef PALM_1
static const char*
soap_strerror(struct soap *soap)
{ int err = soap->errnum;
  *soap->msgbuf = '\0';
  if (err)
  {
#ifndef WIN32
# ifdef HAVE_STRERROR_R
#  ifdef _GNU_SOURCE
    return strerror_r(err, soap->msgbuf, sizeof(soap->msgbuf)); /* GNU-specific */
#  else
    strerror_r(err, soap->msgbuf, sizeof(soap->msgbuf)); /* XSI-compliant */
#  endif
# else
    return strerror(err);
# endif
#else
#ifndef UNDER_CE
    DWORD len;
    *soap->msgbuf = '\0';
    len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)soap->msgbuf, (DWORD)sizeof(soap->msgbuf), NULL);
#else
    DWORD i, len;
    *soap->msgbuf = '\0';
    len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, (LPTSTR)soap->msgbuf, (DWORD)(sizeof(soap->msgbuf)/sizeof(TCHAR)), NULL);
    for (i = 0; i <= len; i++)
    { if (((TCHAR*)soap->msgbuf)[i] < 0x80)
        soap->msgbuf[i] = (char)((TCHAR*)soap->msgbuf)[i];
      else
        soap->msgbuf[i] = '?';
    }
#endif
#endif
  }
  else
  { int rt = soap->recv_timeout, st = soap->send_timeout;
#ifndef WITH_LEAN
    int ru = ' ', su = ' ';
#endif
    soap_strcpy(soap->msgbuf, sizeof(soap->msgbuf), "message transfer interrupted");
    if (rt || st)
      soap_strcpy(soap->msgbuf + 28, sizeof(soap->msgbuf) - 28, " or timed out");
#ifndef WITH_LEAN
    if (rt < 0)
    { rt = -rt;
      ru = 'u';
    }
    if (st < 0)
    { st = -st;
      su = 'u';
    }
    if (rt)
    { size_t l = strlen(soap->msgbuf);
      (SOAP_SNPRINTF(soap->msgbuf + l, sizeof(soap->msgbuf) - l, 36), " (%d%cs recv delay)", rt, ru);
    }
    if (st)
    { size_t l = strlen(soap->msgbuf);
      (SOAP_SNPRINTF(soap->msgbuf + l, sizeof(soap->msgbuf) - l, 36), " (%d%cs send delay)", st, su);
    }
#endif
  }
  return soap->msgbuf;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_2
static int
soap_set_error(struct soap *soap, const char *faultcode, const char *faultsubcodeQName, const char *faultstring, const char *faultdetailXML, int soaperror)
{ *soap_faultcode(soap) = faultcode;
  if (faultsubcodeQName)
    *soap_faultsubcode(soap) = faultsubcodeQName;
  *soap_faultstring(soap) = faultstring;
  if (faultdetailXML && *faultdetailXML)
  { const char **s = soap_faultdetail(soap);
    if (s)
      *s = faultdetailXML;
  }
  return soap->error = soaperror;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_sender_error(struct soap *soap, const char *faultstring, const char *faultdetailXML, int soaperror)
{ return soap_set_error(soap, soap->version == 2 ? "SOAP-ENV:Sender" : soap->version == 1 ? "SOAP-ENV:Client" : "at source", NULL, faultstring, faultdetailXML, soaperror);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_set_receiver_error(struct soap *soap, const char *faultstring, const char *faultdetailXML, int soaperror)
{ return soap_set_error(soap, soap->version == 2 ? "SOAP-ENV:Receiver" : soap->version == 1 ? "SOAP-ENV:Server" : "is internal", NULL, faultstring, faultdetailXML, soaperror);
}
#endif

/******************************************************************************/
#ifndef PALM_2
static int
soap_copy_fault(struct soap *soap, const char *faultcode, const char *faultsubcodeQName, const char *faultstring, const char *faultdetailXML)
{ char *r = NULL, *s = NULL, *t = NULL;
  if (faultsubcodeQName)
    r = soap_strdup(soap, faultsubcodeQName);
  if (faultstring)
    s = soap_strdup(soap, faultstring);
  if (faultdetailXML)
    t = soap_strdup(soap, faultdetailXML);
  return soap_set_error(soap, faultcode, r, s, t, SOAP_FAULT);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_sender_fault(struct soap *soap, const char *faultstring, const char *faultdetailXML)
{ return soap_sender_fault_subcode(soap, NULL, faultstring, faultdetailXML);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_sender_fault_subcode(struct soap *soap, const char *faultsubcodeQName, const char *faultstring, const char *faultdetailXML)
{ return soap_copy_fault(soap, soap->version == 2 ? "SOAP-ENV:Sender" : soap->version == 1 ? "SOAP-ENV:Client" : "at source", faultsubcodeQName, faultstring, faultdetailXML);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_receiver_fault(struct soap *soap, const char *faultstring, const char *faultdetailXML)
{ return soap_receiver_fault_subcode(soap, NULL, faultstring, faultdetailXML);
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
int
SOAP_FMAC2
soap_receiver_fault_subcode(struct soap *soap, const char *faultsubcodeQName, const char *faultstring, const char *faultdetailXML)
{ return soap_copy_fault(soap, soap->version == 2 ? "SOAP-ENV:Receiver" : soap->version == 1 ? "SOAP-ENV:Server" : "is internal", faultsubcodeQName, faultstring, faultdetailXML);
}
#endif

/******************************************************************************/
#ifndef PALM_2
#ifndef WITH_NOSTDLIB
SOAP_FMAC1
void
SOAP_FMAC2
soap_print_fault(struct soap *soap, FILE *fd)
{ if (soap_check_state(soap))
    fprintf(fd, "Error: soap struct state not initialized with soap_init\n");
  else if (soap->error)
  { const char **c, *v = NULL, *s, *d;
    c = soap_faultcode(soap);
    if (!*c)
      soap_set_fault(soap);
    if (soap->version == 2)
      v = soap_check_faultsubcode(soap);
    s = *soap_faultstring(soap);
    d = soap_check_faultdetail(soap);
    fprintf(fd, "%s%d fault %s [%s]\n\"%s\"\nDetail: %s\n", soap->version ? "SOAP 1." : "Error ", soap->version ? (int)soap->version : soap->error, *c, v ? v : "no subcode", s ? s : "[no reason]", d ? d : "[no detail]");
  }
}
#endif
#endif

/******************************************************************************/
#ifdef __cplusplus
#ifndef WITH_LEAN
#ifndef WITH_NOSTDLIB
#ifndef WITH_COMPAT
SOAP_FMAC1
void
SOAP_FMAC2
soap_stream_fault(struct soap *soap, std::ostream& os)
{ if (soap_check_state(soap))
    os << "Error: soap struct state not initialized with soap_init\n";
  else if (soap->error)
  { const char **c, *v = NULL, *s, *d;
    c = soap_faultcode(soap);
    if (!*c)
      soap_set_fault(soap);
    if (soap->version == 2)
      v = soap_check_faultsubcode(soap);
    s = *soap_faultstring(soap);
    d = soap_check_faultdetail(soap);
    os << (soap->version ? "SOAP 1." : "Error ")
       << (soap->version ? (int)soap->version : soap->error)
       << " fault " << *c
       << "[" << (v ? v : "no subcode") << "]"
       << std::endl
       << "\"" << (s ? s : "[no reason]") << "\""
       << std::endl
       << "Detail: " << (d ? d : "[no detail]")
       << std::endl;
  }
}
#endif
#endif
#endif
#endif

/******************************************************************************/
#ifndef WITH_LEAN
#ifndef WITH_NOSTDLIB
SOAP_FMAC1
char*
SOAP_FMAC2
soap_sprint_fault(struct soap *soap, char *buf, size_t len)
{ if (soap_check_state(soap))
  { soap_strcpy(buf, len, "Error: soap struct not initialized with soap_init");
  }
  else if (soap->error)
  { const char **c, *v = NULL, *s, *d;
    c = soap_faultcode(soap);
    if (!*c)
      soap_set_fault(soap);
    if (soap->version == 2)
      v = soap_check_faultsubcode(soap);
    s = *soap_faultstring(soap);
    d = soap_check_faultdetail(soap);
    (SOAP_SNPRINTF(buf, len, strlen(*c) + strlen(v) + strlen(s) + strlen(d) + 72), "%s%d fault %s [%s]\n\"%s\"\nDetail: %s\n", soap->version ? "SOAP 1." : "Error ", soap->version ? (int)soap->version : soap->error, *c, v ? v : "no subcode", s ? s : "[no reason]", d ? d : "[no detail]");
  }
  return buf;
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
#ifndef WITH_NOSTDLIB
SOAP_FMAC1
void
SOAP_FMAC2
soap_print_fault_location(struct soap *soap, FILE *fd)
{
#ifndef WITH_LEAN
  int i, j, c1, c2;
  if (soap->error && soap->error != SOAP_STOP && soap->bufidx <= soap->buflen && soap->buflen > 0 && soap->buflen <= sizeof(soap->buf))
  { i = (int)soap->bufidx - 1;
    if (i <= 0)
      i = 0;
    c1 = soap->buf[i];
    soap->buf[i] = '\0';
    if ((int)soap->buflen >= i + 1024)
      j = i + 1023;
    else
      j = (int)soap->buflen - 1;
    c2 = soap->buf[j];
    soap->buf[j] = '\0';
    fprintf(fd, "%s%c\n<!-- ** HERE ** -->\n", soap->buf, c1);
    if (soap->bufidx < soap->buflen)
      fprintf(fd, "%s\n", soap->buf + soap->bufidx);
    soap->buf[i] = (char)c1;
    soap->buf[j] = (char)c2;
  }
#else
  (void)soap;
  (void)fd;
#endif
}
#endif
#endif

/******************************************************************************/
#ifndef PALM_1
SOAP_FMAC1
int
SOAP_FMAC2
soap_register_plugin_arg(struct soap *soap, int (*fcreate)(struct soap*, struct soap_plugin*, void*), void *arg)
{ struct soap_plugin *p;
  int r;
  if (!(p = (struct soap_plugin*)SOAP_MALLOC(soap, sizeof(struct soap_plugin))))
    return soap->error = SOAP_EOM;
  p->id = NULL;
  p->data = NULL;
  p->fcopy = NULL;
  p->fdelete = NULL;
  r = fcreate(soap, p, arg);
  if (!r && p->fdelete)
  { p->next = soap->plugins;
    soap->plugins = p;
    DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Registered '%s' plugin\n", p->id));
    return SOAP_OK;
  }
  DBGLOG(TEST, SOAP_MESSAGE(fdebug, "Could not register plugin '%s': plugin returned error %d (or fdelete callback not set)\n", p->id ? p->id : "?", r));
  SOAP_FREE(soap, p);
  return r;
}
#endif

/******************************************************************************/
#ifndef PALM_1
static void *
fplugin(struct soap *soap, const char *id)
{ struct soap_plugin *p;
  for (p = soap->plugins; p; p = p->next)
    if (p->id == id || !strcmp(p->id, id))
      return p->data;
  return NULL;
}
#endif

/******************************************************************************/
#ifndef PALM_2
SOAP_FMAC1
void *
SOAP_FMAC2
soap_lookup_plugin(struct soap *soap, const char *id)
{ return soap->fplugin(soap, id);
}
#endif

/******************************************************************************/
#ifdef __cplusplus
}
#endif

/******************************************************************************\
 *
 *	C++ soap struct methods
 *
\******************************************************************************/

#ifdef __cplusplus
soap::soap()
{ soap_init(this);
  /* no logs to prevent leaks when user calls soap_init() on this context */
  soap_set_test_logfile(this, NULL);
  soap_set_sent_logfile(this, NULL);
  soap_set_recv_logfile(this, NULL);
}
#endif

/******************************************************************************/
#ifdef __cplusplus
soap::soap(soap_mode m)
{ soap_init1(this, m);
}
#endif

/******************************************************************************/
#ifdef __cplusplus
soap::soap(soap_mode im, soap_mode om)
{ soap_init2(this, im, om);
}
#endif

/******************************************************************************/
#ifdef __cplusplus
soap::soap(const struct soap& soap)
{ soap_copy_context(this, &soap);
}
#endif

/******************************************************************************/
#ifdef __cplusplus
struct soap& soap::operator=(const struct soap& soap)
{ soap_copy_context(this, &soap);
  return *this;
}
#endif

/******************************************************************************/
#ifdef __cplusplus
soap::~soap()
{ soap_done(this);
}
#endif

/******************************************************************************/
