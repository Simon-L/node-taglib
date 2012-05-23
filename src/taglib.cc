/*
 * Copyright (C) 2009 Nikhil Marathe ( nsm.nikhil@gmail.com )
 * This file is distributed under the MIT License. Please see
 * LICENSE for details
 */
#include "taglib.h"

#include <errno.h>
#include <string.h>

#include <v8.h>
#include <node_buffer.h>

#include <asffile.h>
#include <mpegfile.h>
#include <vorbisfile.h>
#include <flacfile.h>
#include <oggflacfile.h>
#include <mpcfile.h>
#include <mp4file.h>
#include <wavpackfile.h>
#include <speexfile.h>
#include <trueaudiofile.h>
#include <aifffile.h>
#include <wavfile.h>
#include <apefile.h>
#include <modfile.h>
#include <s3mfile.h>
#include <itfile.h>
#include <xmfile.h>

#include "audioproperties.h"
#include "tag.h"
#include "bufferstream.h"

using namespace v8;
using namespace node;
using namespace node_taglib;

namespace node_taglib {
int CreateFileRefPath(TagLib::FileName path, TagLib::FileRef **ref) {
    TagLib::FileRef *f = NULL;
    int error = 0;
    if (!TagLib::File::isReadable(path)) {
        error = EACCES;
    }
    else {
        f = new TagLib::FileRef(path);

        if ( f->isNull() || !f->tag() )
        {
            error = EINVAL;
            delete f;
        }
    }

    if (error != 0)
        *ref = NULL;
    else
        *ref = f;

    return error;
}

int CreateFileRefFile(TagLib::File *file, TagLib::FileRef **ref) {
    TagLib::FileRef *f = NULL;
    int error = 0;
    if (file == NULL) {
        *ref = NULL;
        return EBADF;
    }

    f = new TagLib::FileRef(file);

    if (f->isNull() || !f->tag())
    {
        error = EINVAL;
        delete f;
    }

    if (error != 0)
        *ref = NULL;
    else
        *ref = f;

    return error;
}

Handle<String> ErrorToString(int error) {
    HandleScope scope;
    std::string err;

    switch (error) {
        case EACCES:
            err = "File is not readable";
            break;

        case EINVAL:
            err = "Failed to extract tags";
            break;

        case EBADF:
            err = "Unknown file format (check format string)";
            break;

        default:
            err = "Unknown error";
            break;
    }

    return scope.Close(String::New(err.c_str(), err.length()));
}

v8::Handle<v8::Value> AsyncReadFile(const v8::Arguments &args) {
    HandleScope scope;

    if (args.Length() < 1) {
        return ThrowException(String::New("Expected string or buffer as first argument"));
    }

    if (args[0]->IsString()) {
        if (args.Length() < 2 || !args[1]->IsFunction())
            return ThrowException(String::New("Expected callback function as second argument"));

    }
    else if (Buffer::HasInstance(args[0])) {
        if (args.Length() < 2 || !args[1]->IsString())
            return ThrowException(String::New("Expected string 'format' as second argument"));
        if (args.Length() < 3 || !args[2]->IsFunction())
            return ThrowException(String::New("Expected callback function as third argument"));
    }
    else {
        return ThrowException(String::New("Expected string or buffer as first argument"));
    }

    AsyncBaton *baton = new AsyncBaton;
    baton->path = 0;
    baton->format = TagLib::String::null;
    baton->stream = 0;

    baton->request.data = baton;
    if (args[0]->IsString()) {
        String::Utf8Value path(args[0]->ToString());
        baton->path = strdup(*path);
        baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));

    }
    else {
        baton->format = NodeStringToTagLibString(args[1]->ToString());
        baton->stream = new BufferStream(args[0]->ToObject());
        baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
    }
    baton->error = 0;

    uv_queue_work(uv_default_loop(), &baton->request, AsyncReadFileDo, AsyncReadFileAfter);

    return Undefined();
}

void AsyncReadFileDo(uv_work_t *req) {
    AsyncBaton *baton = static_cast<AsyncBaton*>(req->data);

    TagLib::FileRef *f;

    if (baton->path) {
        baton->error = node_taglib::CreateFileRefPath(baton->path, &f);
    }
    else {
        assert(baton->stream);
        TagLib::File *file = 0;
        baton->format = baton->format.upper();
        if (baton->format == "MPEG")
            file = new TagLib::MPEG::File(baton->stream, TagLib::ID3v2::FrameFactory::instance());
        else if (baton->format == "OGG")
            file = new TagLib::Ogg::Vorbis::File(baton->stream);
        else if (baton->format == "OGG/FLAC")
            file = new TagLib::Ogg::FLAC::File(baton->stream);
        else if (baton->format == "FLAC")
            file = new TagLib::FLAC::File(baton->stream, TagLib::ID3v2::FrameFactory::instance());
        else if (baton->format == "MPC")
            file = new TagLib::MPC::File(baton->stream);
        else if (baton->format == "WV")
            file = new TagLib::WavPack::File(baton->stream);
        else if (baton->format == "SPX")
            file = new TagLib::Ogg::Speex::File(baton->stream);
        else if (baton->format == "TTA")
            file = new TagLib::TrueAudio::File(baton->stream);
        else if (baton->format == "MP4")
            file = new TagLib::MP4::File(baton->stream);
        else if (baton->format == "ASF")
            file = new TagLib::ASF::File(baton->stream);
        else if (baton->format == "AIFF")
            file = new TagLib::RIFF::AIFF::File(baton->stream);
        else if (baton->format == "WAV")
            file = new TagLib::RIFF::WAV::File(baton->stream);
        else if (baton->format == "APE")
            file = new TagLib::APE::File(baton->stream);
        // module, nst and wow are possible but uncommon baton->formatensions
        else if (baton->format == "MOD")
            file = new TagLib::Mod::File(baton->stream);
        else if (baton->format == "S3M")
            file = new TagLib::S3M::File(baton->stream);
        else if (baton->format == "IT")
            file = new TagLib::IT::File(baton->stream);
        else if (baton->format == "XM")
            file = new TagLib::XM::File(baton->stream);

        baton->error = node_taglib::CreateFileRefFile(file, &f);
    }

    if (baton->error == 0) {
        baton->fileRef = f;
    }
}

void AsyncReadFileAfter(uv_work_t *req) {
    AsyncBaton *baton = static_cast<AsyncBaton*>(req->data);
    if (baton->error) {
        Local<Object> error = Object::New();
        error->Set(String::New("code"), Integer::New(baton->error));
        error->Set(String::New("message"), ErrorToString(baton->error));
        Handle<Value> argv[] = { error, Null(), Null() };
        baton->callback->Call(Context::GetCurrent()->Global(), 3, argv);
    }
    else {
        // read the data, put it in objects and delete the fileref
        TagLib::Tag *tag = baton->fileRef->tag();
        Local<Object> tagObj = Object::New();
        if (!tag->isEmpty()) {
            tagObj->Set(String::New("album"), TagLibStringToString(tag->album()));
            tagObj->Set(String::New("artist"), TagLibStringToString(tag->artist()));
            tagObj->Set(String::New("comment"), TagLibStringToString(tag->comment()));
            tagObj->Set(String::New("genre"), TagLibStringToString(tag->genre()));
            tagObj->Set(String::New("title"), TagLibStringToString(tag->title()));
            tagObj->Set(String::New("track"), Integer::New(tag->track()));
            tagObj->Set(String::New("year"), Integer::New(tag->year()));
        }

        TagLib::AudioProperties *props = baton->fileRef->audioProperties();
        Local<Object> propsObj = Object::New();
        if (props) {
            propsObj->Set(String::New("length"), Integer::New(props->length()));
            propsObj->Set(String::New("bitrate"), Integer::New(props->bitrate()));
            propsObj->Set(String::New("sampleRate"), Integer::New(props->sampleRate()));
            propsObj->Set(String::New("channels"), Integer::New(props->channels()));
        }

        Handle<Value> argv[] = { Null(), tagObj, propsObj };
        baton->callback->Call(Context::GetCurrent()->Global(), 3, argv);

        delete baton->fileRef;
        delete baton;
        baton = NULL;
    }
}

Handle<Value> TagLibStringToString( TagLib::String s )
{
    if(s.isEmpty()) {
        return Null();
    }
    else {
        TagLib::ByteVector str = s.data(TagLib::String::UTF16);
        // Strip the Byte Order Mark of the input to avoid node adding a UTF-8
        // Byte Order Mark
        return String::New((uint16_t *)str.mid(2,str.size()-2).data(), s.size());
    }
}

TagLib::String NodeStringToTagLibString( Local<Value> s )
{
    if(s->IsNull()) {
        return TagLib::String::null;
    }
    else {
        String::Utf8Value str(s->ToString());
        return TagLib::String(*str, TagLib::String::UTF8);
    }
}

Handle<Value> AddResolvers(const Arguments &args)
{
    for (int i = 0; i < args.Length(); i++) {
        Local<Value> arg = args[i];
        if (arg->IsFunction()) {
            Persistent<Function> resolver = Persistent<Function>::New(Local<Function>::Cast(arg));
            TagLib::FileRef::addFileTypeResolver(new CallbackResolver(resolver));
        }
    }
}

CallbackResolver::CallbackResolver(Persistent<Function> func)
    : TagLib::FileRef::FileTypeResolver()
    , resolverFunc(func)
    // the constructor is always called in the v8 thread
#ifdef _WIN32
    , created_in(GetCurrentThreadId())
#else
    , created_in(pthread_self())
#endif
{
}

void CallbackResolver::invokeResolverCb(uv_async_t *handle, int status)
{
    AsyncResolverBaton *baton = (AsyncResolverBaton *) handle->data;
    invokeResolver(baton);
    uv_mutex_unlock(&baton->mutex);
    uv_close((uv_handle_t*)&baton->request, 0);
}

void CallbackResolver::invokeResolver(AsyncResolverBaton *baton)
{
    HandleScope scope;
    Handle<Value> argv[] = { TagLibStringToString(baton->fileName) };
    Local<Value> ret = baton->resolver->resolverFunc->Call(Context::GetCurrent()->Global(), 1, argv);
    if (!ret->IsString()) {
        baton->type = TagLib::String::null;
    }
    else {
        baton->type = NodeStringToTagLibString(ret->ToString()).upper();
    }
}

TagLib::File *CallbackResolver::createFile(TagLib::FileName fileName, bool readAudioProperties, TagLib::AudioProperties::ReadStyle audioPropertiesStyle) const
{
    AsyncResolverBaton baton;
    baton.request.data = (void *) &baton;
    baton.resolver = this;
    baton.fileName = fileName;

#ifdef _WIN32
    if (created_at != GetCurrentThreadId()) {
#else
    if (created_in != pthread_self()) {
#endif
        uv_async_init(uv_default_loop(), &baton.request, invokeResolverCb);
        uv_mutex_init(&baton.mutex);

        uv_mutex_lock(&baton.mutex);
        uv_async_send(&baton.request);
        uv_mutex_lock(&baton.mutex);
    }
    else {
        invokeResolver(&baton);
    }

    TagLib::File *file = 0;
    if (baton.type == "MPEG")
        file = new TagLib::MPEG::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "OGG")
        file = new TagLib::Ogg::Vorbis::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "OGG/FLAC")
        file = new TagLib::Ogg::FLAC::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "FLAC")
        file = new TagLib::FLAC::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "MPC")
        file = new TagLib::MPC::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "WV")
        file = new TagLib::WavPack::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "SPX")
        file = new TagLib::Ogg::Speex::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "TTA")
        file = new TagLib::TrueAudio::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "MP4")
        file = new TagLib::MP4::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "ASF")
        file = new TagLib::ASF::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "AIFF")
        file = new TagLib::RIFF::AIFF::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "WAV")
        file = new TagLib::RIFF::WAV::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "APE")
        file = new TagLib::APE::File(fileName, readAudioProperties, audioPropertiesStyle);
    // module, nst and wow are possible but uncommon baton.typeensions
    else if (baton.type == "MOD")
        file = new TagLib::Mod::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "S3M")
        file = new TagLib::S3M::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "IT")
        file = new TagLib::IT::File(fileName, readAudioProperties, audioPropertiesStyle);
    else if (baton.type == "XM")
        file = new TagLib::XM::File(fileName, readAudioProperties, audioPropertiesStyle);

#ifdef _WIN32
    if (created_at != GetCurrentThreadId()) {
#else
    if (created_in != pthread_self()) {
#endif
        uv_mutex_unlock(&baton.mutex);
        uv_mutex_destroy(&baton.mutex);
    }

    return file;
}
}

extern "C" {

static void
init (Handle<Object> target)
{
    HandleScope scope;

#ifdef TAGLIB_WITH_ASF
    target->Set(String::NewSymbol("WITH_ASF"), v8::True());
#else
    target->Set(String::NewSymbol("WITH_ASF"), v8::False());
#endif

#ifdef TAGLIB_WITH_MP4
    target->Set(String::NewSymbol("WITH_MP4"), v8::True());
#else
    target->Set(String::NewSymbol("WITH_MP4"), v8::False());
#endif

    NODE_SET_METHOD(target, "read", AsyncReadFile);
    NODE_SET_METHOD(target, "addResolvers", AddResolvers);
    Tag::Initialize(target);
}

NODE_MODULE(taglib, init)
}