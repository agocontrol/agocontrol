#ifndef SCHEMA_H
#define SCHEMA_H

#include <boost/filesystem.hpp>

using namespace qpid::messaging;
using namespace qpid::types;
namespace fs = ::boost::filesystem;

qpid::types::Variant::List mergeList(qpid::types::Variant::List a, qpid::types::Variant::List b);
qpid::types::Variant::Map mergeMap(qpid::types::Variant::Map a, qpid::types::Variant::Map b);
Variant::Map parseSchema(const fs::path &filename);

#endif
