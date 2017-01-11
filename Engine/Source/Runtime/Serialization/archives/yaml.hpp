#pragma once

//#include "ReflectClass.hpp"
#include <yaml-cpp/yaml.h>
#include <boost/uuid/uuid_io.hpp>

namespace FishEngine
{
	template<typename T>
	struct emittable : std::conditional_t<
		std::is_arithmetic<T>::value,
		std::true_type, std::false_type>
	{};


	class YAMLOutputArchive
	{
	public:
		YAMLOutputArchive(YAML::Emitter & emitter) : m_emitter(emitter)
		{

		}
        
        YAMLOutputArchive(YAMLOutputArchive const &) = delete;
		YAMLOutputArchive& operator = (YAMLOutputArchive const &) = delete;

		~YAMLOutputArchive() noexcept = default;
        
		template<typename T, std::enable_if_t<!emittable<T>::value, int> = 0>
		YAMLOutputArchive & operator << (T const & t)
		{
			prologue(*this, t);
			Save(*this, t);
			epilogue(*this, t);
			return *this;
		}

		template<typename T, std::enable_if_t<emittable<T>::value, int> = 0>
		YAMLOutputArchive & operator << (T const & t)
		{
			m_emitter << t;
			return *this;
		}


		YAMLOutputArchive & operator << (std::string const & t)
		{
			m_emitter << t;
			return *this;
		}

		YAMLOutputArchive & operator << (const char* t)
		{
			m_emitter << t;
			return *this;
		}

		void SerializeObject(std::shared_ptr<Object> const & obj)
		{
			if (obj == nullptr)
			{
				(*this) << nullptr;
				return;
			}

			//std::cout << "SerializeObject: " << obj->ClassName() << std::endl;

			auto guid = obj->GetGUID();
			auto find_result = m_serialized.find(guid);
			if (find_result != m_serialized.end() && find_result->second)
			{
				if (m_isInsideDoc)
					(*this) << obj->GetGUID();
				return;
			}

			if (m_isInsideDoc)
			{
				m_objectsToBeSerialized.push_back(obj);
                //std::cout << "SerializeObject: push, size=" << m_objectsToBeSerialized.size() << std::endl;
				(*this) << obj->GetGUID();
			}
			else
			{
                m_serialized[obj->GetGUID()] = true;
				SetManipulator(YAML::BeginDoc);
				SetManipulator(YAML::BeginMap);
				m_emitter << obj->ClassName();
				//this->operator<<(*obj);
				DynamicSerializeObject(*this, obj);
				SetManipulator(YAML::EndMap);
				SetManipulator(YAML::EndDoc);
			}
		}

		void SetManipulator(YAML::EMITTER_MANIP value)
		{
            if (value == YAML::BeginMap)
            {
                if (m_ignoreNextBeginCount > 0)
                {
                    //m_ignoreNextBeginCount--;
                }
                else
                {
                    m_emitter << value;
                }
                return;
            }
            if (value == YAML::EndMap)
            {
                if (m_ignoreNextBeginCount > 0)
                {
                    m_ignoreNextBeginCount--;
                }
                else
                {
                    m_emitter << value;
                }
                return;
            }
            
            
            m_emitter << value;
            
			if (value == YAML::BeginDoc)
			{
				m_isInsideDoc = true;
			}
			else if (value == YAML::EndDoc)
			{
                //std::cout << "EndDoc size=" << m_objectsToBeSerialized.size() << std::endl;
				m_isInsideDoc = false;
				if (!m_objectsToBeSerialized.empty())
				{
					auto obj = m_objectsToBeSerialized.back();
					m_objectsToBeSerialized.pop_back();
					SerializeObject(obj);
				}
			}
		}
        
        void IgnoreNextBeginMap()
        {
            m_ignoreNextBeginCount++;
        }
        
//        void IgnoreNextEndMap()
//        {
//            m_ignoreNextEndCount++;
//        }
	
	private:
		YAML::Emitter & m_emitter;
		std::map<UUID, bool> m_serialized;
		std::vector<std::shared_ptr<Object>> m_objectsToBeSerialized;
        int m_ignoreNextBeginCount = 0;
        //int m_ignoreNextEndCount = 0;
		bool m_isInsideDoc = false;
	};

    template <class T>
    inline void Save(YAMLOutputArchive & archive, base_class<T> const & t)
    {
        // call Save() directly to avoid prologue and epilogue
        Save(archive, t.base_ref);
    }
    
	template <class T, std::enable_if_t<std::is_enum<T>::value, int> = 0>
	inline void Save(YAMLOutputArchive & archive, T const & t)
	{
		archive << static_cast<uint32_t>(t);
	}

	inline void Save(YAMLOutputArchive & archive, std::nullptr_t const & t)
	{
		archive << make_nvp("fileId", 0);
	}

	inline void prologue(YAMLOutputArchive & archive, std::nullptr_t const & t)
	{
		archive.SetManipulator(YAML::Flow);
		archive.SetManipulator(YAML::BeginMap);
	}

	template <class T>
	inline void Save(YAMLOutputArchive & archive, SizeTag<T> const & t)
	{
		// empty
	}

	template <class T>
	inline void Save(YAMLOutputArchive & archive, std::list<T> const & t)
	{
		for (auto & x : t)
			archive << x;
	}
    
    template<typename T>
    static void Save (YAMLOutputArchive& archive, std::vector<T> const & v)
    {
        //archive << v.size();
        for (auto & x : v)
            archive << x;
    }


	/************************************************************************/
	/* std::shared_ptr                                                      */
	/************************************************************************/
	template<typename T, std::enable_if_t<std::is_base_of<Object, T>::value, int> = 0>
	static void Save(YAMLOutputArchive& archive, std::shared_ptr<T> const & t)
	{
		archive.SerializeObject(t);
	}

	template<typename T, std::enable_if_t<!std::is_base_of<Object, T>::value, int> = 0>
	static void Save(YAMLOutputArchive& archive, std::shared_ptr<T> const & t) = delete;

	/************************************************************************/
	/* std::weak_ptr                                                        */
	/************************************************************************/
	template<typename T>
	inline void Save(YAMLOutputArchive& archive, std::weak_ptr<T> const & v)
	{
		auto t = v.lock();
		archive.SerializeObject(t);
	}

	/************************************************************************/
	/* UUID                                                                 */
	/************************************************************************/
	inline void Save(YAMLOutputArchive & archive, FishEngine::UUID const & t)
	{
		archive << make_nvp("fileId", boost::uuids::to_string(t));
	}

	inline void prologue(YAMLOutputArchive & archive, FishEngine::UUID const & t)
	{
		archive.SetManipulator(YAML::Flow);
		archive.SetManipulator(YAML::BeginMap);
	}

	//inline void epilogue(YAMLOutputArchive& archive, FishEngine::UUID const & t)
	//{
	//	archive.m_emitter << YAML::EndMap;
	//}

	template <class T>
	inline void prologue(YAMLOutputArchive & archive, T const & t)
	{
		//archive.m_emitter << YAML::Block;
		archive.SetManipulator(YAML::BeginMap);
	}

	template <class T>
	inline void epilogue(YAMLOutputArchive& archive, T const & t)
	{
		archive.SetManipulator(YAML::EndMap);
	}
    
    template <class T>
    inline void prologue(YAMLOutputArchive & archive, base_class<T> const & t)
    {
        //archive.SetManipulator(YAML::BeginMap);
        //archive.IgnoreNextBeginMap();
    }
    
    template <class T>
    inline void epilogue(YAMLOutputArchive& archive, base_class<T> const & t)
    {
        //archive.SetManipulator(YAML::EndMap);
    }


	inline void prologue(YAMLOutputArchive& archive, Vector3 const & t)
	{
		archive.SetManipulator(YAML::Flow);
		archive.SetManipulator(YAML::BeginMap);
	}

	inline void prologue(YAMLOutputArchive& archive, Vector4 const & t)
	{
		archive.SetManipulator(YAML::Flow);
		archive.SetManipulator(YAML::BeginMap);
	}

	inline void prologue(YAMLOutputArchive& archive, Quaternion const & t)
	{
		archive.SetManipulator(YAML::Flow);
		archive.SetManipulator(YAML::BeginMap);
	}

	template<typename T>
	inline void prologue(YAMLOutputArchive& archive, std::list<T> const & t)
	{
		if (t.empty())
			archive.SetManipulator(YAML::Flow);
		archive.SetManipulator(YAML::BeginSeq);
	}

	template <class T>
	inline void epilogue(YAMLOutputArchive& archive, std::list<T> const & t)
	{
		archive.SetManipulator(YAML::EndSeq);
	}
    
    template<typename T>
    inline void prologue(YAMLOutputArchive& archive, std::vector<T> const & t)
    {
        if (t.empty())
            archive.SetManipulator(YAML::Flow);
        archive.SetManipulator(YAML::BeginSeq);
    }
    
    template <class T>
    inline void epilogue(YAMLOutputArchive& archive, std::vector<T> const & t)
    {
        archive.SetManipulator(YAML::EndSeq);
    }


	template <class T>
	inline void prologue(YAMLOutputArchive & archive, NameValuePair<T> const & t)
	{
		//archive.m_emitter << YAML::BeginMap;
	}

	template <class T>
	inline void epilogue(YAMLOutputArchive& archive, NameValuePair<T> const & t)
	{
		//archive.m_emitter << YAML::EndMap;
	}

	template <class T>
	inline void prologue(YAMLOutputArchive & archive, std::weak_ptr<T> const & t)
	{
		//archive.m_emitter << YAML::BeginMap;
	}

	template <class T>
	inline void epilogue(YAMLOutputArchive& archive, std::weak_ptr<T> const & t)
	{
		//archive.m_emitter << YAML::EndMap;
	}

	template <class T>
	inline void prologue(YAMLOutputArchive & archive, std::shared_ptr<T> const & t)
	{
		//archive.m_emitter << YAML::BeginMap;
	}

	template <class T>
	inline void epilogue(YAMLOutputArchive& archive, std::shared_ptr<T> const & t)
	{
		//archive.m_emitter << YAML::EndMap;
	}
}
