#pragma once

#include "Shared/Reflection/ReflectionExport.h"
#include "Shared/RPC/RpcReflectionExportAdapter.h"

namespace Helianthus::RPC
{

inline void ExportReflectionToRpc()
{
    RpcReflectionExportAdapter Adapter;
    Helianthus::Reflection::ExportReflection(Adapter);
}

} // namespace Helianthus::RPC


