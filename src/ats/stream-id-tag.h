#ifndef STREAM_ID_TAG_H
#define STREAM_ID_TAG_H

#include "ns3/tag.h"
#include "ns3/core-module.h"

namespace ns3
{

    /**
     * \brief Tag used to carry the streamHandle identifier across layers
     * (e.g., from Receive/Bridge processing to the ATS queuing mechanism in SendFrom).
     */
    class StreamIdTag : public Tag
    {
    public:
        /**
         * \brief Get the TypeId of this class.
         * \return The TypeId.
         */
        static TypeId GetTypeId(void)
        {
            static TypeId tid = TypeId("ns3::StreamIdTag")
                                    .SetParent<Tag>()
                                    .SetGroupName("Tsn")
                                    .AddConstructor<StreamIdTag>();
            return tid;
        }

        /**
         * \brief Default Constructor. Initializes the handle to 0.
         */
        StreamIdTag() : m_streamHandle(0) {}

        /**
         * \brief Constructor with stream handle initialization.
         * \param streamHandle The identifier for the TSN stream.
         */
        StreamIdTag(uint32_t streamHandle) : m_streamHandle(streamHandle) {}

        /**
         * \brief Destructor.
         */
        virtual ~StreamIdTag() {}

        // --- Mandatory methods inherited from ns3::Tag ---

        virtual TypeId GetInstanceTypeId(void) const override
        {
            return GetTypeId();
        }

        virtual uint32_t GetSerializedSize(void) const override
        {
            return sizeof(uint32_t); // The tag only stores a 32-bit integer
        }

        virtual void Serialize(TagBuffer i) const override
        {
            i.WriteU32(m_streamHandle);
        }

        virtual void Deserialize(TagBuffer i) override
        {
            m_streamHandle = i.ReadU32();
        }

        virtual void Print(std::ostream &os) const override
        {
            os << "StreamHandle=" << m_streamHandle;
        }

        // --- Getters and Setters ---

        /**
         * \brief Retrieves the streamHandle stored in the tag.
         * \return The stream handle value.
         */
        uint32_t GetStreamHandle(void) const
        {
            return m_streamHandle;
        }

        /**
         * \brief Updates the streamHandle stored in the tag.
         * \param streamHandle The new handle to assign.
         */
        void SetStreamHandle(uint32_t streamHandle)
        {
            m_streamHandle = streamHandle;
        }

    private:
        uint32_t m_streamHandle; //!< Unique identifier of the TSN stream (stream handle)
    };

} // namespace ns3

#endif // STREAM_ID_TAG_H