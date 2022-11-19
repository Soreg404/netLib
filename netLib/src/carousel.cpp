#include "netlib.h"

net::Carousel::Carousel(size_t cap) {
	m_buffer.reserve(cap);
	m_buffer.resize(cap);
	m_capacity = cap;
}

net::Carousel::~Carousel() {
}

size_t net::Carousel::push(size_t nsize, const void *buffer) {
	size_t pushSize = nsize >= (m_capacity - m_size) ? m_capacity - m_size : nsize;
	for(size_t i = 0; i < pushSize; i++) this->m_buffer[iTransl(m_size + i)] = reinterpret_cast<const char *>(buffer)[i];
	m_size += pushSize;
	return pushSize;
}

size_t net::Carousel::pull(size_t nsize, void *buffer) {
	size_t pullSize = nsize >= m_size ? m_size : nsize;
	if(buffer)
		for(size_t i = 0; i < pullSize; i++) reinterpret_cast<char *>(buffer)[i] = this->m_buffer[iTransl(i)];
	m_head = iTransl(pullSize);
	m_size -= pullSize;
	return pullSize;
}

char &net::Carousel::at(size_t index) {
	return m_buffer.at(iTransl(index));
}

size_t net::Carousel::iTransl(size_t index) const {
	return (m_head + index) % m_capacity;
}