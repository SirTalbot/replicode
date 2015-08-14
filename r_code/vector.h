//	vector.h
//
//	Author: Eric Nivel
//
//	BSD license:
//	Copyright (c) 2010, Eric Nivel
//	All rights reserved.
//	Redistribution and use in source and binary forms, with or without
//	modification, are permitted provided that the following conditions are met:
//
//   - Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   - Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   - Neither the name of Eric Nivel nor the
//     names of their contributors may be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
//	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
//	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//	DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
//	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef r_code_vector_h
#define r_code_vector_h

#include <vector>
#include <cstddef>

namespace r_code {

/// Auto-resizing vector
template<typename T> class vector {
public:
    vector() {}
    ~vector() { }
    size_t size() const {
        return m_vector.size();
    }
    T &operator [](size_t i) {
        if (i >= size()) {
            m_vector.resize(i + 1);
        }
        return m_vector.at(i);
    }
    const T &operator [](size_t i) const
    {
        return m_vector[i];
    }
    void push_back(T t) {
        m_vector.push_back(t);
    }
    std::vector<T> *as_std() {
        return &m_vector;
    }

    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;
    iterator begin() {
        return m_vector->begin();
    }
    const_iterator begin() const {
        return m_vector.begin();
    }
    const_iterator cbegin() const {
        return m_vector.cbegin();
    }
    iterator end() {
        return m_vector.end();
    }
    const_iterator end() const {
        return m_vector.end();
    }
    const_iterator cend() const {
        return m_vector.cend();
    }

private:
    std::vector<T> m_vector;
};

}


#endif
