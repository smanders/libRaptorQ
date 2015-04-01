/*
 * Copyright (c) 2015, Luca Fulchir<luca@fulchir.it>, All rights reserved.
 *
 * This file is part of "libRaptorQ".
 *
 * libRaptorQ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * libRaptorQ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and a copy of the GNU Lesser General Public License
 * along with libRaptorQ.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RAPTORQ_HPP
#define RAPTORQ_HPP

/////////////////////
//
//	These templates are just a wrapper around the
//	functionalities offered by the RaptorQ::Impl namespace
//	So if you want to see what the algorithm looks like,
//	you are in the wrong place
//
/////////////////////

#include "Interleaver.hpp"
#include "De_Interleaver.hpp"
#include "Encoder.hpp"
#include "Decoder.hpp"
#include <map>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace RaptorQ {

template <typename Rnd_It, typename Out_It>
class RAPTORQ_API Encoder;

template <typename Rnd_It, typename Out_It>
class RAPTORQ_API Symbol
{
public:
	Symbol (Encoder<Rnd_It, Out_It> *enc, const uint32_t esi,
															const uint8_t sbn)
		: _enc (enc), _esi (esi), _sbn (sbn) {}

	uint64_t operator() (Out_It &start, const Out_It end)
	{
		return _enc->encode (start, end, _esi, _sbn);
	}

private:
	Encoder<Rnd_It, Out_It> *_enc;
	const uint32_t _esi;
	const uint8_t _sbn;
};

template <typename Rnd_It, typename Out_It>
class RAPTORQ_API Symbol_Iterator :
		public std::iterator<std::input_iterator_tag, Symbol<Rnd_It, Out_It>>
{
public:
	Symbol_Iterator (Encoder<Rnd_It, Out_It> *enc, const uint32_t esi,
															const uint8_t sbn)
		: _enc (enc), _esi (esi), _sbn (sbn) {}
	Symbol<Rnd_It, Out_It> operator*()
	{
		return Symbol<Rnd_It, Out_It> (_enc, _esi, _sbn);
	}
	Symbol_Iterator<Rnd_It, Out_It>& operator++()
	{
		++_esi;
		return *this;
	}
	Symbol_Iterator operator++ (const int i) const
	{
		Symbol_Iterator<Rnd_It, Out_It> ret (_esi + i, _sbn);
		return ret;
	}
	bool operator== (const Symbol_Iterator<Rnd_It, Out_It> it) const
	{
		return it._esi == _esi && it._sbn == _sbn;
	}
	bool operator!= (const Symbol_Iterator<Rnd_It, Out_It> it) const
	{
		return it._esi != _esi || it._sbn != _sbn;
	}
private:
	Encoder<Rnd_It, Out_It> *_enc;
	uint32_t _esi;
	const uint8_t _sbn;
};

template <typename Rnd_It, typename Out_It>
class RAPTORQ_API Block
{
public:
	Block (Encoder<Rnd_It, Out_It> *enc, const uint16_t symbols,
															const uint8_t sbn)
		: _enc (enc), _symbols (symbols), _sbn (sbn) {}

	Symbol_Iterator<Rnd_It, Out_It> begin_source() const
	{
		return Symbol_Iterator<Rnd_It, Out_It> (_enc, 0, _sbn);
	}
	Symbol_Iterator<Rnd_It, Out_It> end_source() const
	{
		return Symbol_Iterator<Rnd_It, Out_It> (_enc, _symbols, _sbn);
	}
	Symbol_Iterator<Rnd_It, Out_It> begin_repair() const
	{
		return Symbol_Iterator<Rnd_It, Out_It> (_enc, _symbols, _sbn);
	}
	Symbol_Iterator<Rnd_It, Out_It> end_repair (const uint32_t max_repair)
																		const
	{
		uint32_t max_r = max_repair;
		if (max_repair > std::pow (2, 20) - _symbols)
			max_r = std::pow (2, 20) - _symbols;
		return Symbol_Iterator<Rnd_It, Out_It> (_enc, _symbols + max_r,_sbn);
	}

private:
	Encoder<Rnd_It, Out_It> * _enc;
	const uint16_t _symbols;
	const uint8_t _sbn;
};

template <typename Rnd_It, typename Out_It>
class RAPTORQ_API Block_Iterator :
		public std::iterator<std::input_iterator_tag, Block<Rnd_It, Out_It>>
{
public:
	Block_Iterator (Encoder<Rnd_It, Out_It> *enc, const Impl::Partition part,
																uint8_t sbn)
		:_enc (enc), _part (part), _sbn (sbn) {}
	Block<Rnd_It, Out_It> operator*()
	{
		if (_sbn > _part.num (0))
			return Block<Rnd_It, Out_It> (_enc, _part.size (0), _sbn);
		return Block<Rnd_It, Out_It> (_enc, _part.size (1), _sbn);
	}
	Block_Iterator& operator++()
	{
		++_sbn;
		return *this;
	}
	Block_Iterator operator++ (const int i) const
	{
		Block_Iterator ret = *this;
		ret._sbn += i;
		return ret;
	}
	bool operator== (const Block_Iterator it) const
	{
		return it._sbn == _sbn;
	}
	bool operator!= (const Block_Iterator it) const
	{
		return it._sbn != _sbn;
	}
private:
	Encoder<Rnd_It, Out_It> *_enc;
	const Impl::Partition _part;
	uint8_t _sbn;
};


static const uint64_t max_data = 946270874880;

typedef uint64_t OTI_Common_Data;
typedef uint32_t OTI_Scheme_Specific_Data;

template <typename Rnd_It, typename Out_It>
class RAPTORQ_API Encoder
{
public:

	Encoder (const Rnd_It data_from, const Rnd_It data_to,
											const uint16_t min_subsymbol_size,
											const uint16_t symbol_size,
											const size_t max_memory)
		: _data_from (data_from), _data_to (data_to),
											_symbol_size (symbol_size),
											_min_subsymbol (min_subsymbol_size),
											_mem (max_memory)
	{
		IS_RANDOM(Rnd_It, "RaptorQ::Encoder");
		IS_OUTPUT(Out_It, "RaptorQ::Encoder");
		// max size: between 2^39 and 2^40
		if (data_to - data_from > max_data)
			return;
		interleave = std::unique_ptr<Impl::Interleaver<Rnd_It>> (
								new Impl::Interleaver<Rnd_It> (_data_from, _data_to,
														_min_subsymbol, _mem,
														_symbol_size));
	}

	Block_Iterator<Rnd_It, Out_It> begin ()
	{
		return Block_Iterator<Rnd_It, Out_It> (this,
												interleave->get_partition(), 0);
	}
	Block_Iterator<Rnd_It, Out_It> end ()
	{
		auto part = interleave->get_partition();
		return Block_Iterator<Rnd_It, Out_It> (this, part,
													part.num(0) + part.num(1));
	}

	bool operator()() const { return interleave != nullptr; }
	OTI_Common_Data OTI_Common() const;
	OTI_Scheme_Specific_Data OTI_Scheme_Specific() const;

	void precompute (const uint8_t threads, const bool background);
	size_t precompute_max_memory ();
	uint64_t encode (Out_It &output, const Out_It end, const uint32_t esi,
															const uint8_t sbn);
	// id: 8-bit sbn + 24 bit esi
	uint64_t encode (Out_It &output, const Out_It end, const uint32_t &id);
	void free (const uint8_t sbn);
	uint8_t blocks() const;
	uint32_t block_size (const uint8_t sbn) const;
	uint16_t symbol_size() const;
	uint16_t symbols (const uint8_t sbn) const;
	uint32_t max_repair (const uint8_t sbn) const;
private:
	class RAPTORQ_LOCAL Locked_Encoder
	{
	public:
		Locked_Encoder (const Impl::Interleaver<Rnd_It> &symbols,
															const uint8_t SBN)
			:_enc (symbols, SBN)
		{}
		std::mutex _mtx;
		Impl::Encoder<Rnd_It, Out_It> _enc;
	};
	const uint16_t _symbol_size;
	const Rnd_It _data_from, _data_to;
	std::unique_ptr<Impl::Interleaver<Rnd_It>> interleave = nullptr;
	std::map<uint8_t, std::shared_ptr<Locked_Encoder>> encoders;
	const size_t _mem;
	std::mutex _mtx;
	const uint16_t _min_subsymbol;

	static void precompute_block_all (Encoder<Rnd_It, Out_It> *obj,
														const uint8_t threads);
	static void precompute_thread (Encoder<Rnd_It, Out_It> *obj, uint8_t *sbn,
													const uint8_t single_sbn);
};


template <typename In_It, typename Out_It>
class RAPTORQ_API Decoder
{
public:
	// using shared pointers to avoid locking too much or
	// worrying about deleting used stuff.
	using Dec_ptr = std::shared_ptr<RaptorQ::Impl::Decoder<In_It>>;

	// rfc 6330, pg 6
	// easy explanation for OTI_* comes next.
	// we do NOT use bitfields as compilators are not actually forced to put
	// them in any particular order. meaning tey're useless.
	//
	//union OTI_Common_Data {
	//	uint64_t raw;
	//	struct {
	//		uint64_t size:40;
	//		uint8_t reserved:8;
	//		uint16_t symbol_size:16;
	//	};
	//};

	//union OTI_Scheme_Specific_Data {
	//	uint32_t raw;
	//	struct {
	//		uint8_t source_blocks;
	//		uint16_t sub_blocks;
	//		uint8_t	alignment;
	//	};
	//};
	Decoder (const OTI_Common_Data common,const OTI_Scheme_Specific_Data scheme)
	{
		IS_INPUT(In_It, "RaptorQ::Decoder");
		IS_OUTPUT(Out_It, "RaptorQ::Decoder");

		// see the above commented bitfields for quick reference
		_symbol_size = static_cast<uint16_t> (common);
		_sub_blocks = static_cast<uint16_t> (scheme >> 8);
		_blocks = static_cast<uint8_t> (common >> 24);
		//	(common >> 24) == total file size
		const uint64_t size = common >> 24;
		if (size > max_data)
			return;

		using T_in = typename std::iterator_traits<In_It>::value_type;
		const uint64_t total_symbols = static_cast<uint64_t> (ceil (
									static_cast<double> (size * sizeof(T_in)) /
										static_cast<double> (_symbol_size)));

		part = Impl::Partition (total_symbols,
										static_cast<uint8_t> (scheme >> 24));
		//FIXME: check that the OSI and "part" agree on the data.
	}

	Decoder (uint16_t symbol_size, uint16_t sub_blocks, uint8_t blocks)
		:_symbol_size (symbol_size), _sub_blocks (sub_blocks), _blocks (blocks)
	{}

	uint32_t decode (Out_It &start, const Out_It end);
	uint32_t decode (Out_It &start, const Out_It end, const uint8_t sbn);
	// id: 8-bit sbn + 24 bit esi
	bool add_symbol (In_It &start, const In_It end, const uint32_t id);
	bool add_symbol (In_It &start, const In_It end, const uint32_t esi,
															const uint8_t sbn);
	void free (const uint8_t sbn);
	uint8_t blocks() const;
	uint32_t block_size (const uint8_t sbn) const;
	uint16_t symbol_size() const;
	uint16_t symbols (const uint8_t sbn) const;
private:
	Impl::Partition part;
	uint16_t _symbol_size, _sub_blocks;
	uint8_t _blocks;
	std::map<uint8_t, Dec_ptr> decoders;
	std::mutex _mtx;
};





/////////////////
//
// Encoder
//
/////////////////

template <typename Rnd_It, typename Out_It>
OTI_Common_Data Encoder<Rnd_It, Out_It>::OTI_Common() const
{
	OTI_Common_Data ret;
	// first 40 bits: data length.
	ret = (_data_to - _data_from) << 24;
	// 8 bits: reserved
	// last 16 bits: symbol size
	ret += _symbol_size;

	return ret;
}

template <typename Rnd_It, typename Out_It>
OTI_Scheme_Specific_Data Encoder<Rnd_It, Out_It>::OTI_Scheme_Specific() const
{
	OTI_Scheme_Specific_Data ret;
	// 8 bit: source blocks
	ret = interleave->blocks() << 24;
	// 16 bit: sub-blocks number (N)
	ret += interleave->sub_blocks() << 8;
	// 8 bit: alignment
	ret += sizeof(typename std::iterator_traits<Rnd_It>::value_type);

	return ret;
}

template <typename Rnd_It, typename Out_It>
size_t Encoder<Rnd_It, Out_It>::precompute_max_memory ()
{
	// give a good estimate on the amount of memory neede for the precomputation
	// of one block;
	// this will help you understand how many concurrent precomputations
	// you want to do :)

	if (interleave == nullptr)
		return 0;

	uint16_t symbols = interleave->source_symbols(0);

	uint16_t K_idx;
	for (K_idx = 0; K_idx < Impl::K_padded.size(); ++K_idx) {
		if (symbols < Impl::K_padded[K_idx])
			break;
	}
	if (K_idx == Impl::K_padded.size())
		return 0;

	auto S_H = Impl::S_H_W[K_idx];
	uint16_t matrix_cols = Impl::K_padded[K_idx] + std::get<0> (S_H) +
															std::get<1> (S_H);

	// Rough memory estimate: Matrix A, matrix X (=> *2) and matrix D.
	return matrix_cols * matrix_cols * 2 + _symbol_size * matrix_cols;
}

template <typename Rnd_It, typename Out_It>
void Encoder<Rnd_It, Out_It>::precompute_thread (Encoder<Rnd_It, Out_It> *obj,
													uint8_t *sbn,
													const uint8_t single_sbn)
{
	// if "sbn" pointer is NOT nullptr, than we are a thread from
	// from a precompute_block_all. This means that we need to update
	// the value of sbn as soon as we get our work.
	//
	// if sbn == nullptr, then we have been called to work on a single
	// sbn, and not from "precompute_block_all".
	// This means we work on "single_sbn", and do not touch "sbn"

	uint8_t *sbn_ptr = sbn;
	if (sbn_ptr == nullptr)
		sbn_ptr = const_cast<uint8_t*> (&single_sbn);
	// call this from a thread, precomput all block symbols
	while (*sbn_ptr < obj->interleave->blocks()) {
		obj->_mtx.lock();
		if (*sbn_ptr >= obj->interleave->blocks()) {
			obj->_mtx.unlock();
			return;
		}
		auto it = obj->encoders.find (*sbn_ptr);
		if (it == obj->encoders.end()) {
			bool success;
			std::tie (it, success) = obj->encoders.insert ({*sbn_ptr,
					std::make_shared<Locked_Encoder> (*obj->interleave, *sbn_ptr)
														});
		}
		auto enc_ptr = it->second;
		bool locked = enc_ptr->_mtx.try_lock();
		if (sbn != nullptr)
			++(*sbn);
		obj->_mtx.unlock();
		if (locked) {	// if not locked, someone else is already waiting
						// on this. so don't do the same work twice.
			enc_ptr->_enc.generate_symbols();
			enc_ptr->_mtx.unlock();
		}
		if (sbn == nullptr)
			return;
	}
}

template <typename Rnd_It, typename Out_It>
void Encoder<Rnd_It, Out_It>::precompute (const uint8_t threads,
														const bool background)
{
	if (background) {
		std::thread t (precompute_block_all, this, threads);
		t.detach();
	} else {
		return precompute_block_all (this, threads);
	}
}

template <typename Rnd_It, typename Out_It>
void Encoder<Rnd_It, Out_It>::precompute_block_all (
												Encoder<Rnd_It, Out_It> *obj,
												const uint8_t threads)
{
	// precompute all intermediate symbols, do it with more threads.
	if (obj->interleave == nullptr)
		return;
	std::vector<std::thread> t;
	uint8_t spawned = threads - 1;
	if (spawned == 0)
		spawned = std::thread::hardware_concurrency();

	if (spawned > 0)
		t.reserve (spawned);
	uint8_t sbn = 0;

	// spawn n-1 threads
	for (uint8_t id = 0; id < spawned; ++id)
		t.emplace_back (precompute_thread, obj, &sbn, 0);

	// do the work ourselves
	precompute_thread (obj, &sbn, 0);

	// join other threads
	for (uint8_t id = 0; id < spawned; ++id)
		t[id].join();
}

template <typename Rnd_It, typename Out_It>
uint64_t Encoder<Rnd_It, Out_It>::encode (Out_It &output, const Out_It end,
															const uint32_t &id)
{
	const uint32_t mask_8 = static_cast<uint32_t> (std::pow (2, 8)) - 1;
	const uint32_t mask = ~(mask_8 << 24);

	return encode (output, end, id & mask, static_cast<uint8_t> (id & mask_8));
}

template <typename Rnd_It, typename Out_It>
uint64_t Encoder<Rnd_It, Out_It>::encode (Out_It &output, const Out_It end,
															const uint32_t esi,
															const uint8_t sbn)
{
	if (sbn >= interleave->blocks())
		return false;
	_mtx.lock();
	auto it = encoders.find (sbn);
	if (it == encoders.end()) {
		bool success;
		std::tie (it, success) = encoders.emplace (sbn,
						std::make_shared<Locked_Encoder> (*interleave, sbn));
		std::thread background (precompute_thread, this, nullptr, sbn);
		background.detach();
	}
	auto enc_ptr = it->second;
	_mtx.unlock();
	if (esi >= interleave->source_symbols (sbn)) {
		// make sure we generated the intermediate symbols
		enc_ptr->_mtx.lock();
		enc_ptr->_enc.generate_symbols();
		enc_ptr->_mtx.unlock();
	}

	return enc_ptr->_enc.Enc (esi, output, end);
}


template <typename Rnd_It, typename Out_It>
void Encoder<Rnd_It, Out_It>::free (const uint8_t sbn)
{
	_mtx.lock();
	auto it = encoders.find (sbn);
	if (it != encoders.end())
		encoders.erase (it);
	_mtx.unlock();
}

template <typename Rnd_It, typename Out_It>
uint8_t Encoder<Rnd_It, Out_It>::blocks() const
{
	return interleave->blocks();
}

template <typename Rnd_It, typename Out_It>
uint32_t Encoder<Rnd_It, Out_It>::block_size (const uint8_t sbn) const
{
	return interleave->source_symbols (sbn) * interleave->symbol_size();
}

template <typename Rnd_It, typename Out_It>
uint16_t Encoder<Rnd_It, Out_It>::symbol_size() const
{
	if (interleave == nullptr)
		return 0;
	return interleave->symbol_size();
}

template <typename Rnd_It, typename Out_It>
uint16_t Encoder<Rnd_It, Out_It>::symbols (const uint8_t sbn) const
{
	if (interleave == nullptr)
		return 0;
	return interleave->source_symbols (sbn);
}

template <typename Rnd_It, typename Out_It>
uint32_t Encoder<Rnd_It, Out_It>::max_repair (const uint8_t sbn) const
{
	return std::pow (2, 20) - interleave->source_symbols (sbn);
}
/////////////////
//
// Decoder
//
/////////////////

template <typename In_It, typename Out_It>
void Decoder<In_It, Out_It>::free (const uint8_t sbn)
{
	_mtx.lock();
	auto it = decoders.find(sbn);
	if (it != decoders.end())
		decoders.erase(it);
	_mtx.unlock();
}

template <typename In_It, typename Out_It>
bool Decoder<In_It, Out_It>::add_symbol (In_It &start, const In_It end,
															const uint32_t id)
{
	union extract {
		uint32_t raw;
		struct {
			uint8_t sbn;
			uint32_t esi:24;
		};
	} extracted;

	extracted.raw = id;

	return add_symbol (start, end, extracted.esi, extracted.sbn);
}

template <typename In_It, typename Out_It>
bool Decoder<In_It, Out_It>::add_symbol (In_It &start, const In_It end,
															const uint32_t esi,
															const uint8_t sbn)
{
	if (sbn >= _blocks)
		return false;
	_mtx.lock();
	auto it = decoders.find (sbn);
	if (it == decoders.end()) {
		const uint16_t symbols = sbn < part.num (0) ?
													part.size(0) : part.size(1);
		decoders.insert ({sbn, std::make_shared<Impl::Decoder<In_It>> (
													symbols, _symbol_size)});
		it = decoders.find (sbn);
	}
	auto dec = it->second;
	_mtx.unlock();

	return dec->add_symbol (start, end, esi);
}

template <typename In_It, typename Out_It>
uint32_t Decoder<In_It, Out_It>::decode (Out_It &start, const Out_It end)
{
	// TODO: incomplete decoding

	bool missing = false;
	for (uint8_t sbn = 0; sbn < _blocks; ++sbn) {
		_mtx.lock();
		auto it = decoders.find (sbn);
		if (it == decoders.end()) {
			missing = true;
			continue;
		}
		auto dec = it->second;
		_mtx.unlock();

		if (!dec->decode())
			return 0;
	}
	if (missing)
		return 0;
	uint32_t written = 0;
	for (uint8_t sbn = 0; sbn < _blocks; ++sbn) {
		_mtx.lock();
		auto it = decoders.find (sbn);
		if (it == decoders.end())
			return written;
		auto dec = it->second;
		_mtx.unlock();
		Impl::De_Interleaver<Out_It> de_interleaving (
											dec->get_symbols(), _sub_blocks);
		written += de_interleaving (start, end);
	}
	return written;
}

template <typename In_It, typename Out_It>
uint32_t Decoder<In_It, Out_It>::decode (Out_It &start, const Out_It end,
															const uint8_t sbn)
{
	if (sbn >= _blocks)
		return 0;

	_mtx.lock();
	auto it = decoders.find (sbn);
	if (it == decoders.end()) {
		_mtx.unlock();
		return 0;
	}
	auto dec = it->second;
	_mtx.unlock();

	if (!dec->decode())
		return 0;

	Impl::De_Interleaver<Out_It> de_interleaving (dec->get_symbols(),
																_sub_blocks);
	return de_interleaving (start, end);
}

template <typename In_It, typename Out_It>
uint8_t Decoder<In_It, Out_It>::blocks() const
{
	return part.num (0) + part.num (1);
}

template <typename In_It, typename Out_It>
uint32_t Decoder<In_It, Out_It>::block_size (const uint8_t sbn) const
{
	if (sbn < part.num (0)) {
		return part.size (0) * _symbol_size;
	} else if (sbn - part.num (0) < part.num (1)) {
		return part.size (1) * _symbol_size;
	}
	return 0;
}

template <typename In_It, typename Out_It>
uint16_t Decoder<In_It, Out_It>::symbol_size() const
{
	return _symbol_size;
}
template <typename In_It, typename Out_It>
uint16_t Decoder<In_It, Out_It>::symbols (const uint8_t sbn) const
{
	if (sbn < part.num (0)) {
		return part.size (0);
	} else if (sbn - part.num (0) < part.num (1)) {
		return part.size (1);
	}
	return 0;
}

}	//RaptorQ

#endif
