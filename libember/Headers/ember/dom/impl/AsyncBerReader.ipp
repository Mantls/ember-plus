/*
    libember -- C++ 03 implementation of the Ember+ Protocol
    Copyright (C) 2012  L-S-B Broadcast Technologies GmbH

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __LIBEMBER_DOM_IMPL_ASYNCBERREADER_IPP
#define __LIBEMBER_DOM_IMPL_ASYNCBERREADER_IPP

#include <stdexcept>
#include "../../util/Inline.hpp"
#include "../VariantLeaf.hpp"
#include "../Set.hpp"
#include "../Sequence.hpp"
#include "../NodeFactory.hpp"

namespace libember { namespace dom 
{
    LIBEMBER_INLINE
    AsyncBerReader::~AsyncBerReader()
    {
        reset();
    }

    LIBEMBER_INLINE
    void AsyncBerReader::reset()
    {
        while(m_stack.empty() == false)
        {
            AsyncContainer* container = m_stack.top();

            m_stack.pop();
            delete container;
        }

        m_container = 0;

        disposeCurrentTLV();
        reset(DecodeState::Tag);
        resetImpl();
    }

    LIBEMBER_INLINE
    void AsyncBerReader::read(value_type value)
    {
        m_buffer.append(value);

        if (m_container != 0)
            m_container->incrementBytesRead();

        bool isEofOk = false;

        switch(m_decodeState.value())
        {
            case DecodeState::Tag:
                isEofOk = readTagByte(value);
                break;

            case DecodeState::Length:
                isEofOk = readLengthByte(value);
                break;

            case DecodeState::Value:
                isEofOk = readValueByte(value);
                break;

            case DecodeState::Terminator:
                isEofOk = readTerminatorByte(value);
                break;
        }

        while(m_container != 0 && m_container->eof())
        {
            if (isEofOk == false)
                throw std::runtime_error("Unexpected end of container");

            popContainer();
        }
    }

    LIBEMBER_INLINE
    AsyncBerReader::AsyncBerReader()
        : m_buffer(0)
        , m_valueBuffer(0)
        , m_container(0)
        , m_decodeState(DecodeState::Tag)
        , m_bytesRead(0)
        , m_bytesExpected(0)
        , m_valueLength(0)
        , m_isContainer(false)
        , m_length(0)
        , m_outerLength(0)
    {}

    LIBEMBER_INLINE
    void AsyncBerReader::resetImpl()
    {
    }

    LIBEMBER_INLINE
    AsyncBerReader::size_type AsyncBerReader::length() const
    {
        return m_length;
    }

    LIBEMBER_INLINE
    bool AsyncBerReader::isContainer() const
    {
        return m_isContainer;
    }

    LIBEMBER_INLINE
    dom::Node* AsyncBerReader::decodeNode(dom::NodeFactory const& factory)
    {
        ber::Type const type = ber::Type::fromTag(m_typeTag);
        ber::Tag const tag = m_appTag;

        if (!type.isApplicationDefined())
        {
            if (m_isContainer)
            {
                switch(type.value())
                {
                    case ber::Type::Set:
                        return new dom::Set(tag);

                    case ber::Type::Sequence:
                        return new dom::Sequence(tag);

                    default:
                        return 0;
                }
            }
            else
            {
                switch(type.value())
                {
                    case ber::Type::Boolean:
                        return new dom::VariantLeaf(tag, decode<bool>());

                    case ber::Type::Integer:
                        if (m_length > 4)
                            return new dom::VariantLeaf(tag, decode<long>());
                        else
                            return new dom::VariantLeaf(tag, decode<int>());

                    case ber::Type::Real:
                        return new dom::VariantLeaf(tag, decode<double>());

                    case ber::Type::UTF8String:
                        return new dom::VariantLeaf(tag, decode<std::string>());

                    case ber::Type::RelativeObject:
                        return new dom::VariantLeaf(tag, decode<ber::ObjectIdentifier>());

                    case ber::Type::OctetString:
                        return new dom::VariantLeaf(tag, decode<ber::Octets>());

                    default:
                        break;
                }

                return 0;
            }
        }
        else
        {
            return factory.createApplicationDefinedNode(type, tag);
        }
    }

    LIBEMBER_INLINE
    bool AsyncBerReader::readTagByte(value_type value)
    {
        if (value == 0 && m_bytesRead == 0)
        {
            reset(DecodeState::Terminator);
            return false;
        }

        if (m_bytesRead > 12)
            throw std::runtime_error("Number of tag octets out of bounds");

        if ((m_bytesRead == 0 && (value & 0x1F) != 0x1F)
        ||  (m_bytesRead > 0 && (value & 0x80) == 0))
        {
            if (m_appTag.number() == 0 && m_appTag.preamble() == 0)
            {
                ber::Tag const tag = libember::ber::decode<ber::Tag>(m_buffer);

                m_appTag = tag;
                m_appTag.setContainer(false);
            }
            else
            {
                ber::Tag const tag = libember::ber::decode<ber::Tag>(m_buffer);

                m_isContainer = tag.isContainer();
                m_typeTag = tag;
            }

            reset(DecodeState::Length);
            return false;
        }

        m_bytesRead++;
        return false;
    }

    LIBEMBER_INLINE
    bool AsyncBerReader::readLengthByte(value_type value)
    {
        if (m_bytesExpected == 0)
        {
            if ((value & 0x80) != 0)
            {
                m_bytesExpected = (value & 0x7F) + 1;
            }
            else
            {
                m_bytesExpected = 1;
            }

            if (m_bytesExpected > 5)
                throw std::runtime_error("Number of length octets out of bounds");
        }

        m_bytesRead++;

        if (m_bytesRead == m_bytesExpected)
        {
            ber::Type const type = ber::Type::fromTag(m_typeTag);
            if (type.value() == 0)
            {
                m_outerLength = ber::decode<length_type>(m_buffer).value;

                if (m_outerLength == 0)
                    throw std::runtime_error("Zero outer length encountered");

                reset(DecodeState::Tag);
            }
            else 
            {
                m_length = ber::decode<length_type>(m_buffer).value;

                bool const isEofOk = m_length == 0;
                if (m_isContainer)
                {
                    reset(DecodeState::Tag);
                    containerReady();
                    pushContainer();
                    disposeCurrentTLV();
                    return isEofOk;
                }

                if (m_length == 0)
                {
                    preloadValue();
                }
                else
                {
                    reset(DecodeState::Value);
                }

                return isEofOk;
            }
        }
        return false;
    }

    LIBEMBER_INLINE
    bool AsyncBerReader::readValueByte(value_type)
    {
        if (m_bytesRead == 0)
            m_bytesExpected = m_length;

        m_bytesRead++;

        if (m_bytesRead == m_bytesExpected)
        {
            preloadValue();
            return true;
        }

        return false;
    }

    LIBEMBER_INLINE
    bool AsyncBerReader::readTerminatorByte(value_type value)
    {
        if (m_container == 0
        ||  m_container->length() != length_type::INDEFINITE)
            throw std::runtime_error("Unexpected terminator");

        if (value == 0)
        {
            m_bytesRead++;

            if (m_bytesRead == 3)
            {
                // end of indefinite length container
                if (m_container != 0)
                    m_container->setLength(m_container->bytesRead());

                return true;
            }
        }
        else
        {
            throw std::runtime_error("Non-zero byte in terminator");
        }

        return false;
    }

    LIBEMBER_INLINE
    void AsyncBerReader::reset(DecodeState const& state)
    {
        m_buffer.clear();
        m_decodeState = state;
        m_bytesExpected = 0;
        m_bytesRead = 0;
    }

    LIBEMBER_INLINE
    void AsyncBerReader::preloadValue()
    {
        if (m_valueBuffer.empty() == false)
            m_valueBuffer.clear();

        m_valueBuffer.swap(m_buffer);
        m_valueLength = std::min(m_length, m_valueBuffer.size());

        reset(DecodeState::Tag);
        itemReady();
        disposeCurrentTLV();
    }

    LIBEMBER_INLINE
    void AsyncBerReader::pushContainer()
    {
        AsyncContainer* container = 0;

        try
        {
            container = new AsyncContainer(m_appTag, m_typeTag, m_length);
            m_stack.push(container);
            m_container = container;
        }
        catch( ... )
        {
            if (container != 0)
                delete container;

            throw std::runtime_error("AsyncBerReader::pushContainer failed");
        }
    }

    LIBEMBER_INLINE
    bool AsyncBerReader::popContainer()
    {
        reset(DecodeState::Tag);

        if (m_stack.size() > 0)
        {
            AsyncContainer* container = m_stack.top();
            m_stack.pop();
            m_appTag = container->tag();
            m_typeTag = container->type();
            m_length = container->length();
            m_isContainer = true;

            itemReady();

            if (m_stack.empty())
            {
                m_container = 0;
            }
            else
            {
                m_container = m_stack.top();
                m_container->incrementBytesRead(m_length);
            }

            delete container;
            disposeCurrentTLV();
            return true;
        }
        else
        {
            return false;
        }
    }

    LIBEMBER_INLINE
    void AsyncBerReader::disposeCurrentTLV()
    {
        m_appTag = ber::make_tag(0, 0);
        m_typeTag = ber::make_tag(0, 0);
        m_isContainer = false;
        m_length = 0;
        m_outerLength = 0;
        m_valueBuffer.clear();
        m_valueLength = 0;
    }
}
}

#endif  // __LIBEMBER_DOM_IMPL_ASYNCBERREADER_IPP
