/** **************************************************************************
 * private.h
 * 
 * Copyright 2008 Bryan Ischo <bryan@ischo.com>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the
 *
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ************************************************************************** **/

#ifndef PRIVATE_H
#define PRIVATE_H

#include <curl/curl.h>
#include <curl/multi.h>
#include "libs3.h"


// As specified in S3 documntation
#define META_HEADER_NAME_PREFIX "x-amz-meta-"
#define HOSTNAME "s3.amazonaws.com"


// Derived from S3 documentation

// This is the maximum number of x-amz-meta- headers that could be included in
// a request to S3.  The smallest meta header is" x-amz-meta-n: v".  Since S3
// doesn't count the ": " against the total, the smallest amount of data to
// count for a header would be the length of "x-amz-meta-nv".
#define MAX_META_HEADER_COUNT \
    (S3_MAX_META_HEADER_SIZE / (sizeof(META_HEADER_NAME_PREFIX "nv") - 1))

// This is the maximum number of bytes needed in a "compacted meta header"
// buffer, which is a buffer storing all of the compacted meta headers.
#define COMPACTED_META_HEADER_BUFFER_SIZE \
    (MAX_META_HEADER_COUNT * sizeof(META_HEADER_NAME_PREFIX "n: v"))

// Maximum url encoded key size; since every single character could require
// URL encoding, it's 3 times the size of a key (since each url encoded
// character takes 3 characters: %NN)
#define MAX_URLENCODED_KEY_SIZE (3 * S3_MAX_KEY_SIZE)

// This is the maximum size of a URI that could be passed to S3:
// https://s3.amazonaws.com/${BUCKET}/${KEY}?acl
// 255 is the maximum bucket length
#define MAX_URI_SIZE \
    ((sizeof("https://" HOSTNAME "/") - 1) + 255 + 1 + \
     MAX_URLENCODED_KEY_SIZE + (sizeof("?torrent" - 1)) + 1)

// Maximum size of a canonicalized resource
#define MAX_CANONICALIZED_RESOURCE_SIZE \
    (1 + 255 + 1 + MAX_URLENCODED_KEY_SIZE + (sizeof("?torrent") - 1) + 1)


// Describes a type of HTTP request (these are our supported HTTP "verbs")
typedef enum
{
    HttpRequestTypeGET,
    HttpRequestTypeHEAD,
    HttpRequestTypePUT,
    HttpRequestTypeCOPY,
    HttpRequestTypeDELETE
} HttpRequestType;


// Simple XML callback.
//
// elementPath: is the full "path" of the element; i.e.
// <foo><bar><baz>data</baz><bar><foo> would have 'data' in the element
// foo/bar/baz.
// 
// Return of anything other than S3StatusOK causes the calling
// simplexml_add() function to immediately stop and return the status.
//
// data is passed in as 0 on end of element
typedef S3Status (SimpleXmlCallback)(const char *elementPath, const char *data,
                                     int dataLen, void *callbackDatao);

typedef struct SimpleXml
{
    void *xmlParser;

    SimpleXmlCallback *callback;

    void *callbackData;

    char elementPath[512];

    int elementPathLen;

    S3Status status;
} SimpleXml;


// This completely describes a request.  A RequestParams is not required to be
// allocated from the heap and its lifetime is not assumed to extend beyond
// the lifetime of the function to which it has been passed.
typedef struct RequestParams
{
    // The following are supplied ---------------------------------------------

    // Request type, affects the HTTP verb used
    HttpRequestType httpRequestType;
    // Protocol to use for request
    S3Protocol protocol;
    // URI style to use for request
    S3UriStyle uriStyle;
    // Bucket name, if any
    const char *bucketName;
    // Key, if any
    const char *key;
    // Query params - ready to append to URI (i.e. ?p1=v1?p2=v2)
    const char *queryParams;
    // sub resource, like ?acl, ?location, ?torrent
    const char *subResource;
    // AWS Access Key ID
    const char *accessKeyId;
    // AWS Secret Access Key
    const char *secretAccessKey;
    // Request headers
    const S3RequestHeaders *requestHeaders;
    // Response handler callback
    S3ResponseHandler *handler;
    // The callbacks to make for the data payload of the response
    union {
        S3ListServiceCallback *listServiceCallback;
        S3ListBucketCallback *listBucketCallback;
        S3PutObjectCallback *putObjectCallback;
        S3GetObjectCallback *getObjectCallback;
    } u;
    // Response handler callback data
    void *callbackData;
    // The write callback to be called by curl, if needed; req will be passed
    // in as the Request structure for this request
    size_t (*curlWriteCallback)(void *data, size_t s, size_t n, void *req);
    // The read callback to be called by curl, if needed; req will be passed
    // in as the Request structure for this request
    size_t (*curlReadCallback)(void *data, size_t s, size_t n, void *req);
    // This is the number of bytes that will be provided by the read callback,
    // if the read callback is set
    int64_t readSize;
    

    // The following are computed ---------------------------------------------

    // All x-amz- headers, in normalized form (i.e. NAME: VALUE, no other ws)
    char *amzHeaders[MAX_META_HEADER_COUNT + 2]; // + 2 for acl and date
    // The number of x-amz- headers
    int amzHeadersCount;
    // Storage for amzHeaders (the +256 is for x-amz-acl and x-amz-date)
    char amzHeadersRaw[COMPACTED_META_HEADER_BUFFER_SIZE + 256 + 1];
    // Canonicalized x-amz- headers
    char canonicalizedAmzHeaders[COMPACTED_META_HEADER_BUFFER_SIZE + 256 + 1];
    // URL-Encoded key
    char urlEncodedKey[MAX_URLENCODED_KEY_SIZE + 1];
    // Canonicalized resource
    char canonicalizedResource[MAX_CANONICALIZED_RESOURCE_SIZE + 1];
    // Cache-Control header (or empty)
    char cacheControlHeader[128];
    // Content-Type header (or empty)
    char contentTypeHeader[128];
    // Content-MD5 header (or empty)
    char md5Header[128];
    // Content-Disposition header (or empty)
    char contentDispositionHeader[128];
    // Content-Encoding header (or empty)
    char contentEncodingHeader[128];
    // Expires header (or empty)
    char expiresHeader[128];
    // Authorization header
    char authorizationHeader[128];
} RequestParams;


typedef struct ListServiceXmlCallbackData
{
    char ownerId[256];
    int ownerIdLen;

    char ownerDisplayName[256];
    int ownerDisplayNameLen;

    char bucketName[256];
    int bucketNameLen;

    char creationDate[128];
    int creationDateLen;
} ListServiceXmlCallbackData;


typedef struct ListBucketXmlCallbackData
{

} ListBucketXmlCallbackData;


// This is the stuff associated with a request that needs to be on the heap
// (and thus live while a curl_multi is in use).
typedef struct Request
{
    // True if this request has already been used
    int used;

    // The status of this Request, as will be reported to the user via the
    // complete callback
    S3Status status;

    // The CURL structure driving the request
    CURL *curl;

    // The HTTP headers to use for the curl request
    struct curl_slist *headers;

    // libcurl requires that the uri be stored outside of the curl handle
    char uri[MAX_URI_SIZE + 1];

    // The callback data to pass to all of the callbacks
    void *callbackData;

    // responseHeaders.{requestId,requestId2,contentType,server,eTag} get
    // copied into here.  We allow 128 bytes for each header, plus \0 term.
    char responseHeaderStrings[5 * 129];

    // The length thus far of responseHeaderStrings
    int responseHeaderStringsLen;

    // responseHeaders.metaHeaders strings get copied into here
    char responseMetaHeaderStrings[COMPACTED_META_HEADER_BUFFER_SIZE];

    // The length thus far of metaHeaderStrings
    int responseMetaHeaderStringsLen;

    // Response meta headers
    S3NameValue responseMetaHeaders[MAX_META_HEADER_COUNT];

    // Callback to make when headers are available
    S3ResponseHeadersCallback *headersCallback;

    // The structure to pass to the headers callback
    S3ResponseHeaders responseHeaders;

    // This is set to nonzero after the haders callback has been made
    int headersCallbackMade;

    // The HTTP response code that S3 sent back for this request
    int httpResponseCode;

    // This is the write callback that the user of the request wants to have
    // called back when data is available.
    size_t (*curlWriteCallback)(void *data, size_t s, size_t n, void *req);

    // This is the read callback that the user of the request wants to have
    // called back when data is to be written.
    size_t (*curlReadCallback)(void *data, size_t s, size_t n, void *req);

    // The callback to make when the response has been completely handled
    S3ResponseCompleteCallback *completeCallback;

    // This is nonzero if the error XML parser has been initialized
    int errorXmlParserInitialized;

    // This is the error XML parser
    SimpleXml errorXmlParser;

    // If S3 did send an XML error, this is the parsed form of it
    S3ErrorDetails s3ErrorDetails;

    // These are the buffers used to store the S3Error values
    char s3ErrorCode[1024];
    int s3ErrorCodeLen;

    // These are the buffers used to store the S3Error values
    char s3ErrorMessage[1024];
    int s3ErrorMessageLen;

    // These are the buffers used to store the S3Error values
    char s3ErrorResource[1024];
    int s3ErrorResourceLen;
    
    // These are the buffers used to store the S3Error values
    char s3ErrorFurtherDetails[1024];
    int s3ErrorFurtherDetailsLen;
    
    // The extra details; we support up to 8 of them
    S3NameValue s3ErrorExtraDetails[8];
    // This is the buffer from which the names used in s3ErrorExtraDetails
    // are allocated
    char s3ErrorExtraDetailsNames[512];
    // And this is the length of each element of s3ErrorExtraDetailsNames
    int s3ErrorExtraDetailsNamesLen;
    // These are the buffers of values in the s3ErrorExtraDetails.  They
    // are kept separate so that they can be individually appended to.
    char s3ErrorExtraDetailsValues[8][1024];
    // And these are the individual lengths of each of each element of
    // s3ErrorExtraDetailsValues
    int s3ErrorExtraDetailsValuesLens[8];

    // The following fields are used by the S3 functions themselves, not
    // the request code.

    // xxx rewrite all of this stuff, it's ugly.  Do the following:
    // * Define request as a self-contained API similar to what it is now,
    //   but supporting only get object and put object callbacks
    // * Make the S3 functions that have to do XML parsing allocate their
    //   own callback data, that does the XML parsing, and then passes the
    //   parsed data on
    //
    // Alternately, something like:
    //
    // - An API for preparing a CURL handle to issue an S3 request
    // - Separate APIs for handling response data
    // - The S3 functions tie all of these together, with their own callbacks
    //   for parsing XML data if necessary
    //
    // In either case, break the Request structure up into separate structures
    // with their own APIs (response header parsing, error parsing, etc),
    // and compose them here, instead of having everything stuck into this
    // structure directly, which is ugly.

    // The callbacks to make for the data payload of the response
    union {
        S3ListServiceCallback *listServiceCallback;
        S3ListBucketCallback *listBucketCallback;
        S3PutObjectCallback *putObjectCallback;
        S3GetObjectCallback *getObjectCallback;
    } callback;

    // The data to use during the xml callback
    union {
        ListServiceXmlCallbackData listServiceXmlCallbackData;
        ListBucketXmlCallbackData listBucketXmlCallbackData;
    } data;

    // The XML parser that the write callback will use if it needs it
    SimpleXml dataXmlParser;
    
    // Whoever initializes dataXmlParser has to set this to 1
    int dataXmlParserInitialized;
} Request;


struct S3RequestContext
{
    CURLM *curlm;

    int count;
};


// Mutex functions ------------------------------------------------------------

// Create a mutex.  Returns 0 if none could be created.
struct S3Mutex *mutex_create();

// Lock a mutex
void mutex_lock(struct S3Mutex *mutex);

// Unlock a mutex
void mutex_unlock(struct S3Mutex *mutex);

// Destroy a mutex
void mutex_destroy(struct S3Mutex *mutex);


// Request functions
// ----------------------------------------------------------------------------

// Initialize the API
S3Status request_api_initialize(const char *userAgentInfo);

// Deinitialize the API
void request_api_deinitialize();

// Perform a request; if context is 0, performs the request immediately;
// otherwise, sets it up to be performed by context.
void request_perform(RequestParams *params, S3RequestContext *context);

// Called by the internal request code or internal request context code when a
// curl has finished the request
void request_finish(Request *request, S3Status status);


// Simple XML parsing
// ----------------------------------------------------------------------------

S3Status simplexml_initialize(SimpleXml *simpleXml, 
                              SimpleXmlCallback *callback, void *callbackData);

S3Status simplexml_add(SimpleXml *simpleXml, const char *data, int dataLen);


void simplexml_deinitialize(SimpleXml *simpleXml);


#endif /* PRIVATE_H */
