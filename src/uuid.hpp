#ifndef UUID_HPP_INCLUDED
#define UUID_HPP_INCLUDED

#include <boost/uuid/uuid_generators.hpp>

#include "variant.hpp"

boost::uuids::uuid generate_uuid();
std::string write_uuid(const boost::uuids::uuid& id);
boost::uuids::uuid read_uuid(const std::string& s);

#endif
