#pragma once
#include "Funscript.h"
#include "OFS_StateHandle.h"

struct FunscriptMetadataState
{
    static constexpr auto StateName = "FunscriptMetadata";

    Funscript::Metadata defaultMetadata;

    // Helper function to apply default metadata to a metadata object
    // Only fills in fields that are empty in the target
    inline void ApplyDefaultMetadata(Funscript::Metadata& metadata) const noexcept
    {
        if (metadata.creator.empty() && !defaultMetadata.creator.empty())
            metadata.creator = defaultMetadata.creator;
        if (metadata.script_url.empty() && !defaultMetadata.script_url.empty())
            metadata.script_url = defaultMetadata.script_url;
        if (metadata.video_url.empty() && !defaultMetadata.video_url.empty())
            metadata.video_url = defaultMetadata.video_url;
        if (metadata.description.empty() && !defaultMetadata.description.empty())
            metadata.description = defaultMetadata.description;
        if (metadata.notes.empty() && !defaultMetadata.notes.empty())
            metadata.notes = defaultMetadata.notes;
        if (metadata.license.empty() && !defaultMetadata.license.empty())
            metadata.license = defaultMetadata.license;
        if (metadata.tags.empty() && !defaultMetadata.tags.empty())
            metadata.tags = defaultMetadata.tags;
        if (metadata.performers.empty() && !defaultMetadata.performers.empty())
            metadata.performers = defaultMetadata.performers;
    }

    static inline FunscriptMetadataState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<FunscriptMetadataState>(stateHandle).Get();
    }
};

REFL_TYPE(FunscriptMetadataState)
    REFL_FIELD(defaultMetadata)
REFL_END