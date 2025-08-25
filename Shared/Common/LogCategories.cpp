#include "Common/LogCategory.h"

using namespace Helianthus::Common;

H_DEFINE_LOG_CATEGORY(Net, LogVerbosity::Log);
H_DEFINE_LOG_CATEGORY(Perf, LogVerbosity::Log);

// Message Queue related categories (avoid name clash with classes)
H_DEFINE_LOG_CATEGORY(MQ, LogVerbosity::Log);
H_DEFINE_LOG_CATEGORY(MQPersistence, LogVerbosity::Log);
H_DEFINE_LOG_CATEGORY(MQManager, LogVerbosity::Log);

