#include "neuron/container/node.hpp"
#include "section.h"

#include <catch2/catch.hpp>

#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

using namespace neuron::container;

// We want to check that the tests pass for all of:
// - data_handle<T>
// - data_handle<T> -> T* -> data_handle<T>
// - data_handle<T> -> generic_data_handle -> data_handle<T>
enum struct Transform { None, ViaRawPointer, ViaGenericDataHandle };
template <typename T>
data_handle<T> transform(data_handle<T> handle, Transform type) {
    if (type == Transform::None) {
        return handle;
    } else if (type == Transform::ViaRawPointer) {
        return {static_cast<T*>(handle)};
    } else {
        assert(type == Transform::ViaGenericDataHandle);
        return {generic_data_handle{handle}};
    }
}

TEST_CASE("data_handle<double>", "[Neuron][data_structures][data_handle]") {
    GIVEN("A null handle") {
        data_handle<double> handle{};
        auto const transformation =
            GENERATE(Transform::None, Transform::ViaRawPointer, Transform::ViaGenericDataHandle);
        handle = transform(handle, transformation);
        THEN("Check it is really null") {
            REQUIRE_FALSE(handle);
        }
        THEN("Check it compares equal to a different null pointer") {
            data_handle<double> const other_handle{};
            REQUIRE(handle == other_handle);
        }
    }
    constexpr double magic_voltage_value = 42.;
    GIVEN("A handle wrapping a raw pointer (compatibility mode)") {
        double foo{magic_voltage_value};
        data_handle<double> handle{&foo};
        auto const transformation =
            GENERATE(Transform::None, Transform::ViaRawPointer, Transform::ViaGenericDataHandle);
        handle = transform(handle, transformation);
        THEN("Check it is not null") {
            REQUIRE(handle);
        }
        THEN("Check it does not compare equal to a null handle") {
            data_handle<double> null_handle{};
            REQUIRE(handle != null_handle);
        }
        THEN("Check it compares equal to a different handle wrapping the same raw pointer") {
            data_handle<double> other_handle{&foo};
            REQUIRE(handle == other_handle);
        }
        THEN("Check it yields the right value") {
            REQUIRE(*handle == foo);
        }
        THEN("Check it doesn't claim to be modern") {
            REQUIRE_FALSE(handle.refers_to_a_modern_data_structure());
        }
        THEN("Check it decays to the right raw pointer") {
            auto* foo_ptr = static_cast<double*>(handle);
            REQUIRE(foo_ptr == &foo);
        }
        THEN("Check it prints the right value") {
            std::ostringstream actual, expected{};
            actual << handle;
            expected << "data_handle<double>{raw=" << &foo << '}';
            REQUIRE(actual.str() == expected.str());
        }
    }
    GIVEN("A handle referring to an entry in an SOA container") {
        std::optional<::Node> node{std::in_place};
        node->set_v(magic_voltage_value);
        auto handle = node->v_handle();
        auto const transformation =
            GENERATE(Transform::None, Transform::ViaRawPointer, Transform::ViaGenericDataHandle);
        handle = transform(handle, transformation);
        THEN("Check it is not null") {
            REQUIRE(handle);
        }
        THEN("Check it does not compare equal to a null handle") {
            data_handle<double> null_handle{};
            REQUIRE(handle != null_handle);
        }
        THEN("Check it does not compare equal to a handle in legacy mode") {
            double foo{};
            data_handle<double> foo_handle{&foo};
            REQUIRE(handle != foo_handle);
        }
        THEN("Check it yields the right value") {
            REQUIRE(*handle == magic_voltage_value);
        }
        THEN("Check it claims to be modern") {
            REQUIRE(handle.refers_to_a_modern_data_structure());
        }
        THEN("Check it prints the right value") {
            std::ostringstream actual;
            actual << handle;
            REQUIRE(actual.str() == "data_handle<double>{cont=Node::field::Voltage row=0/1}");
        }
        THEN("Check that deleting the (Node) object it refers to invalidates the handle") {
            node.reset();  // delete the underlying Node object
            REQUIRE_FALSE(handle);
            REQUIRE(handle.refers_to_a_modern_data_structure());
            std::ostringstream actual;
            actual << handle;
            REQUIRE(actual.str() == "data_handle<double>{cont=Node::field::Voltage died/0}");
        }
        THEN(
            "Check that mutating the underlying container while holding a raw pointer has the "
            "expected effect") {
            auto* raw_ptr = static_cast<double*>(handle);
            REQUIRE(raw_ptr);
            REQUIRE(*raw_ptr == magic_voltage_value);
            node.reset();  // delete the underlying Node object, handle is now invalid
            REQUIRE_FALSE(handle);
            REQUIRE(raw_ptr);  // no magic here, we have a dangling pointer
            data_handle<double> new_handle{raw_ptr};
            REQUIRE(new_handle);  // handle refers to no-longer-valid memory, but we can't detect
                                  // that
            REQUIRE(handle != new_handle);
            REQUIRE_FALSE(new_handle.refers_to_a_modern_data_structure());
            // dereferencing raw_ptr is undefined behaviour
        }
    }
}

TEST_CASE("generic_data_handle", "[Neuron][data_structures][generic_data_handle]") {
    GIVEN("A null generic handle") {
        // no default constructor, have to go via a data_handle<T>
        generic_data_handle null_handle{data_handle<double>{}};
        THEN("Check it remembered the double type") {
            REQUIRE(null_handle.type_name() == "double");
        }
        THEN("Check it can be converted back to data_handle<double>") {
            auto const round_trip = static_cast<data_handle<double>>(null_handle);
            REQUIRE_FALSE(round_trip);
            REQUIRE(round_trip == data_handle<double>{});
        }
        THEN("Check it cannot be converted to data_handle<int>") {
            REQUIRE_THROWS(static_cast<data_handle<int>>(null_handle));
        }
    }
    GIVEN("A handle wrapping a raw pointer (compatibility mode)") {
        double foo{};
        data_handle<double> typed_handle{&foo};
        generic_data_handle handle{typed_handle};
        THEN("Check it remembered the double type") {
            REQUIRE(handle.type_name() == "double");
        }
        THEN("Check it can be converted back to data_handle<double>") {
            REQUIRE_NOTHROW(static_cast<data_handle<double>>(handle));
        }
        THEN("Check it cannot be converted to data_handle<int>") {
            REQUIRE_THROWS(static_cast<data_handle<int>>(handle));
        }
    }
}

template <typename Nodes>
std::vector<double> get_voltages(Nodes const& nodes) {
    std::vector<double> ret{};
    std::transform(nodes.begin(), nodes.end(), std::back_inserter(ret), [](auto const& node) {
        return node.v();
    });
    return ret;
}

TEST_CASE("SOA-backed Node structure", "[Neuron][data_structures][node]") {
    GIVEN("A series of nodes with increasing integer voltages") {
        std::vector<double> reference_voltages{};
        std::generate_n(std::back_inserter(reference_voltages), 10, [i = 0]() mutable {
            auto x = i++;
            return x * x;
        });
        std::vector<::Node> nodes{};
        std::transform(reference_voltages.begin(),
                       reference_voltages.end(),
                       std::back_inserter(nodes),
                       [](auto v) {
                           ::Node node{};
                           node.set_v(v);
                           return node;
                       });
        THEN("Check we can read back the same voltages") {
            REQUIRE(get_voltages(nodes) == reference_voltages);
        }
    }
}