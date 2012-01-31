#ifndef OPENVPN_COMPRESS_COMPRESS_H
#define OPENVPN_COMPRESS_COMPRESS_H

#include <openvpn/common/types.hpp>
#include <openvpn/common/exception.hpp>
#include <openvpn/common/rc.hpp>
#include <openvpn/buffer/buffer.hpp>
#include <openvpn/frame/frame.hpp>
#include <openvpn/log/protostats.hpp>

#if defined(OPENVPN_DEBUG_COMPRESS)
#define OPENVPN_LOG_COMPRESS(x) OPENVPN_LOG(x)
#else
#define OPENVPN_LOG_COMPRESS(x)
#endif

namespace openvpn {
  class Compress : public RC<thread_unsafe_refcount>
  {
  public:
    typedef boost::intrusive_ptr<Compress> Ptr;

    // Compression method implemented by underlying compression class.
    // hint should normally be true to compress the data.  If hint is
    // false, the data may be uncompressible or already compressed,
    // so method shouldn't attempt compression.
    virtual void compress(BufferAllocated& buf, const bool hint) = 0;

    // Decompression method implemented by underlying compression class.
    virtual void decompress(BufferAllocated& buf) = 0;

  protected:
    // magic numbers to indicate no compression
    enum {
      NO_COMPRESS      = 0xFA,
      NO_COMPRESS_SWAP = 0xFB, // for better alignment handling, replace this byte with last byte of packet
    };

    Compress(const Frame::Ptr& frame_arg,
	     const ProtoStats::Ptr& stats_arg)
      : frame(frame_arg), stats(stats_arg) {}

    void error(BufferAllocated& buf)
    {
      stats->error(ProtoStats::COMPRESS_ERROR);
      buf.reset_size();
    }

    void do_swap(Buffer& buf, unsigned char op)
    {
      if (buf.size())
	{
	  buf.push_back(buf[0]);
	  buf[0] = op;
	}
      else
	buf.push_back(op);
    }

    void do_unswap(Buffer& buf)
    {
      if (buf.size() >= 2)
	{
	  const unsigned char first = buf.pop_back();
	  buf.push_front(first);
	}
    }

    Frame::Ptr frame;
    ProtoStats::Ptr stats;
  };
}// namespace openvpn

// include compressor implementations here
#include <openvpn/compress/compnull.hpp>
#include <openvpn/compress/compstub.hpp>

#ifdef HAVE_LZO
#include <openvpn/compress/lzo.hpp>
#endif
#ifdef HAVE_LZ4
#include <openvpn/compress/lz4.hpp>
#endif
#ifdef HAVE_SNAPPY
#include <openvpn/compress/snappy.hpp>
#endif

namespace openvpn {
  class CompressContext
  {
  public:
    enum Type {
      NONE,
      COMP_STUB,  // generic compression stub
      ANY,        // placeholder for any method on client, before server assigns it
      ANY_LZO,    // placeholder for LZO or LZO_STUB methods on client, before server assigns it
      LZO,
      LZO_SWAP,
      LZO_STUB,
      LZ4,
      SNAPPY,
    };

    OPENVPN_SIMPLE_EXCEPTION(compressor_unavailable);

    CompressContext() : type_(NONE) {}

    explicit CompressContext(const Type t)
    {
      if (!compressor_available(t))
	throw compressor_unavailable();
      type_ = t;
    }

    unsigned int extra_payload_bytes() const { return type_ == NONE ? 0 : 1; }

    Compress::Ptr new_compressor(const Frame::Ptr& frame, const ProtoStats::Ptr& stats)
    {
      switch (type_)
	{
	case NONE:
	  return new CompressNull(frame, stats);
	case ANY:
	case ANY_LZO:
	case LZO_STUB:
	  return new CompressStub(frame, stats, false);
	case COMP_STUB:
	  return new CompressStub(frame, stats, true);
#ifdef HAVE_LZO
	case LZO:
	  return new CompressLZO(frame, stats, false);
	case LZO_SWAP:
	  return new CompressLZO(frame, stats, true);
#endif
#ifdef HAVE_LZ4
	case LZ4:
	  return new CompressLZ4(frame, stats);
#endif
#ifdef HAVE_SNAPPY
	case SNAPPY:
	  return new CompressSnappy(frame, stats);
#endif
	default:
	  throw compressor_unavailable();
	}
    }

    static bool compressor_available(const Type t)
    {
      switch (t)
	{
	case NONE:
	case ANY:
	case ANY_LZO:
	case LZO_STUB:
	case COMP_STUB:
	  return true;
	case LZO:
	case LZO_SWAP:
#ifdef HAVE_LZO
	  return true;
#else
	  return false;
#endif
	case LZ4:
#ifdef HAVE_LZ4
	  return true;
#else
	  return false;
#endif
	case SNAPPY:
#ifdef HAVE_SNAPPY
	  return true;
#else
	  return false;
#endif
	default:
	  return false;
	}
    }

    const char *peer_info_string() const
    {
      switch (type_)
	{
#ifdef HAVE_LZO
	case LZO:
	  return "IV_LZO=1\n";
	case LZO_SWAP:
	  return "IV_LZO_SWAP=1\n";
#endif
#ifdef HAVE_LZ4
	case LZ4:
	  return "IV_LZ4=1\n";
#endif
#ifdef HAVE_SNAPPY
	case SNAPPY:
	  return "IV_SNAPPY=1\n";
#endif
	case LZO_STUB:
	  return "IV_LZO_STUB=1\n";
	case COMP_STUB:
	  return "IV_COMP_STUB=1\n";
	case ANY:
	  return
#ifdef HAVE_LZO
	    "IV_LZO=1\n"
	    "IV_LZO_SWAP=1\n"
#else
	    "IV_LZO_STUB=1\n"
#endif
#ifdef HAVE_LZ4
	    "IV_LZ4=1\n"
#endif
#ifdef HAVE_SNAPPY
	    "IV_SNAPPY=1\n"
#endif
	    "IV_COMP_STUB=1\n"
	    ;
	case ANY_LZO:
	  return
#ifdef HAVE_LZO
	    "IV_LZO=1\n"
	    "IV_LZO_SWAP=1\n"
#else
	    "IV_LZO_STUB=1\n"
#endif
	    ;
	default:
	  return NULL;
	}
    }

    const char *options_string() const
    {
      switch (type_)
	{
	case LZO:
	case LZO_STUB:
	case SNAPPY:
	case LZ4:
	case LZO_SWAP:
	case COMP_STUB:
	case ANY:
	case ANY_LZO:
	  return "comp-lzo";
	default:
	  return NULL;
	}
    }

    const char *str() const
    {
      switch (type_)
	{
	case LZO:
	  return "LZO";
	case LZO_SWAP:
	  return "LZO_SWAP";
	case LZ4:
	  return "LZ4";
	case SNAPPY:
	  return "SNAPPY";
	case LZO_STUB:
	  return "LZO_STUB";
	case COMP_STUB:
	  return "COMP_STUB";
	case ANY:
	  return "ANY";
	case ANY_LZO:
	  return "ANY_LZO";
	default:
	  return "NONE";
	}
    }

    static Type parse_method(const std::string& method)
    {
      if (method == "lzo")
	return LZO;
      else if (method == "lzo-swap")
	return LZO_SWAP;
      else if (method == "lzo-stub")
	return LZO_STUB;
      else if (method == "lz4")
	return LZ4;
      else if (method == "snappy")
	return SNAPPY;
      else if (method == "stub")
	return COMP_STUB;
      else
	return NONE;
    }

    static void init_static()
    {
#ifdef HAVE_LZO
      CompressLZO::init_static();
#endif
    }

  private:
    Type type_;
    };

} // namespace openvpn

#endif // OPENVPN_COMPRESS_COMPRESS_H