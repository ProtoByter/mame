// license:GPL-2.0+
// copyright-holders:Couriersud


#ifndef PSTREAM_H_
#define PSTREAM_H_

///
/// \file pstream.h
///

#include "palloc.h"
#include "pconfig.h"
#include "pexception.h"
#include "pfmtlog.h"
#include "pstring.h"
#include "pstrutil.h"

#include <array>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <fstream>
#include <type_traits>
#include <vector>

namespace plib {

///
/// \brief: putf8reader_t: reader on top of istream.
///
/// putf8reader_t digests linux & dos/windows text files
///
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class putf8_reader
{
public:

	PCOPYASSIGN(putf8_reader, delete)
	virtual ~putf8_reader() = default;

	putf8_reader(putf8_reader &&rhs) noexcept
	: m_strm(std::move(rhs.m_strm))
	, m_linebuf(std::move(rhs.m_linebuf))
	{
	}

	putf8_reader(plib::unique_ptr<std::istream> &&rhs) noexcept
	: m_strm(std::move(rhs))
	{
	}

	bool eof() const { return m_strm->eof(); }

	bool readline(pstring &line)
	{
		putf8string::code_t c = 0;
		m_linebuf = putf8string("");
		if (!this->readcode(c))
		{
			line = "";
			return false;
		}
		while (true)
		{
			if (c == 10)
				break;
			if (c != 13) // ignore CR
				m_linebuf += putf8string(1, c);
			if (!this->readcode(c))
				break;
		}
		line = m_linebuf;
		return true;
	}

	bool readbyte(std::istream::char_type &b)
	{
		if (m_strm->eof())
			return false;
		m_strm->read(&b, 1);
		return (!m_strm->eof());
	}

	bool readcode(putf8string::traits_type::code_t &c)
	{
		std::array<std::istream::char_type, 4> b{0};
		if (m_strm->eof())
			return false;
		m_strm->read(&b[0], 1);
		if (m_strm->eof())
			return false;
		const std::size_t l = putf8string::traits_type::codelen(reinterpret_cast<putf8string::traits_type::mem_t *>(&b));
		for (std::size_t i = 1; i < l; i++)
		{
			m_strm->read(&b[i], 1);
			if (m_strm->eof())
				return false;
		}
		c = putf8string::traits_type::code(reinterpret_cast<putf8string::traits_type::mem_t *>(&b));
		return true;
	}

	std::istream &stream() { return *m_strm; }
private:
	plib::unique_ptr<std::istream> m_strm;
	putf8string m_linebuf;
};

// -----------------------------------------------------------------------------
// putf8writer_t: writer on top of ostream
// -----------------------------------------------------------------------------

class putf8_writer
{
public:
	explicit putf8_writer(std::ostream *strm) : m_strm(strm) {}

	putf8_writer(putf8_writer &&src) noexcept : m_strm(src.m_strm) {}

	PCOPYASSIGN(putf8_writer, delete)
	putf8_writer &operator=(putf8_writer &&src) = delete;

	virtual ~putf8_writer() = default;

	void writeline(const pstring &line) const
	{
		write(line);
		write(10);
	}

	void write(const pstring &text) const
	{
		// NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
		const putf8string conv_utf8(text);
		m_strm->write(conv_utf8.c_str(), static_cast<std::streamsize>(plib::strlen(conv_utf8.c_str())));
	}

	void write(const pstring::value_type c) const
	{
		pstring t(1,c);
		write(t);
	}

private:
	std::ostream *m_strm;
};

class putf8_fmt_writer : public pfmt_writer_t<putf8_fmt_writer>, public putf8_writer
{
public:

	explicit putf8_fmt_writer(std::ostream *strm)
	: putf8_writer(strm)
	{
	}

	PCOPYASSIGNMOVE(putf8_fmt_writer, delete)

	~putf8_fmt_writer() override = default;

//protected:
	void vdowrite(const pstring &s) const
	{
		write(s);
	}


private:
};

// -----------------------------------------------------------------------------
// pbinary_writer_t: writer on top of ostream
// -----------------------------------------------------------------------------

class pbinary_writer
{
public:
	explicit pbinary_writer(std::ostream &strm) : m_strm(strm) {}
	pbinary_writer(pbinary_writer &&src) noexcept : m_strm(src.m_strm) {}

	PCOPYASSIGN(pbinary_writer, delete)
	pbinary_writer &operator=(pbinary_writer &&src) = delete;

	virtual ~pbinary_writer() = default;

	template <typename T>
	void write(const T &val)
	{
		m_strm.write(reinterpret_cast<const std::ostream::char_type *>(&val), sizeof(T));
	}

	void write(const pstring &s)
	{
		const auto *const sm = reinterpret_cast<const std::ostream::char_type *>(s.c_str());
		const auto sl(static_cast<std::streamsize>(pstring_mem_t_size(s)));
		write(sl);
		m_strm.write(sm, sl);
	}

	template <typename T>
	void write(const std::vector<T> &val)
	{
		const auto sz(static_cast<std::streamsize>(val.size()));
		write(sz);
		m_strm.write(reinterpret_cast<const std::ostream::char_type *>(val.data()), sz * static_cast<std::streamsize>(sizeof(T)));
	}

private:
	std::ostream &m_strm;
};

class pbinary_reader
{
public:
	explicit pbinary_reader(std::istream &strm) : m_strm(strm) {}
	pbinary_reader(pbinary_reader &&src) noexcept : m_strm(src.m_strm) { }

	PCOPYASSIGN(pbinary_reader, delete)
	pbinary_reader &operator=(pbinary_reader &&src) = delete;

	virtual ~pbinary_reader() = default;

	template <typename T>
	void read(T &val)
	{
		m_strm.read(reinterpret_cast<std::istream::char_type *>(&val), sizeof(T));
	}

	void read( pstring &s)
	{
		std::size_t sz = 0;
		read(sz);
		std::vector<plib::string_info<pstring>::mem_t> buf(sz+1);
		m_strm.read(buf.data(), static_cast<std::streamsize>(sz));
		buf[sz] = 0;
		s = pstring(buf.data());
	}

	template <typename T>
	void read(std::vector<T> &val)
	{
		std::size_t sz = 0;
		read(sz);
		val.resize(sz);
		m_strm.read(reinterpret_cast<std::istream::char_type *>(val.data()), static_cast<std::streamsize>(sizeof(T) * sz));
	}

private:
	std::istream &m_strm;
};

inline void copystream(std::ostream &dest, std::istream &src)
{
	// FIXME: optimize
	std::array<std::ostream::char_type, 1024> buf; // NOLINT(cppcoreguidelines-pro-type-member-init)
	while (!src.eof())
	{
		src.read(buf.data(), 1);
		dest.write(buf.data(), 1);
	}
}

///
/// \brief utf8 filename aware ifstream wrapper
///
class ifstream : public std::ifstream
{
public:
#ifdef _WIN32
	template <typename T>
	explicit ifstream(const pstring_t<T> name, ios_base::openmode mode = ios_base::in)
	: std::ifstream(reinterpret_cast<const wchar_t *>(pstring_t<putf16_traits>(name).c_str()), mode)
	{
	}
#else
	template <typename T>
	explicit ifstream(const pstring_t<T> name, ios_base::openmode mode = ios_base::in)
	: std::ifstream(pstring_t<putf8_traits>(name).c_str(), mode)
	{
	}
#endif
};

///
/// \brief utf8 filename aware ofstream wrapper
///
class ofstream : public std::ofstream
{
public:
#ifdef _WIN32
	template <typename T>
	explicit ofstream(const pstring_t<T> name, ios_base::openmode mode = ios_base::in)
	: std::ofstream(reinterpret_cast<const wchar_t *>(pstring_t<putf16_traits>(name).c_str()), mode)
	{
	}
#else
	template <typename T>
	explicit ofstream(const pstring_t<T> name, ios_base::openmode mode = ios_base::in)
	: std::ofstream(pstring_t<putf8_traits>(name).c_str(), mode)
	{
	}
#endif
};


struct perrlogger
{
	template <typename ... Args>
	explicit perrlogger(Args&& ... args)
	{
		h()(std::forward<Args>(args)...); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	}
private:
	static putf8_fmt_writer &h()
	{
		static plib::putf8_fmt_writer perr(&std::cerr);
		return perr;
	}
};

// -----------------------------------------------------------------------------
// c++17 preparation
// -----------------------------------------------------------------------------

namespace filesystem
{

	// FIXME: u8path should return a path object (c++17)

	template< class Source >
	pstring u8path( const Source& source )
	{
		return source;
	}

} // namespace filesystem

} // namespace plib

#endif // PSTREAM_H_
