#include "check.h"
#include "../src/linux/atspi_serializer.h"

int main() {
    using namespace broa11y::atspi;

    // 1. Primitive encoding
    CHECK_EQ(Serializer::encode_string("hello"), "\"hello\"");
    CHECK_EQ(Serializer::encode_int32(-42), "-42");
    CHECK_EQ(Serializer::encode_uint32(100), "100");
    CHECK_EQ(Serializer::encode_bool(true), "true");
    CHECK_EQ(Serializer::encode_bool(false), "false");

    // 2. Struct encoding
    Reference ref{.sender = ":1.42", .path = "/org/a11y/atspi/accessible/root"};
    CHECK_EQ(Serializer::encode_reference(ref), "(\":1.42\", \"/org/a11y/atspi/accessible/root\")");

    Rect r{.x = 10, .y = 20, .width = 300, .height = 400};
    CHECK_EQ(Serializer::encode_rect(r), "(10, 20, 300, 400)");

    Point pt{.x = 5, .y = 15};
    CHECK_EQ(Serializer::encode_point(pt), "(5, 15)");

    TextSlice slice{.text = "world", .start_offset = 6, .end_offset = 11};
    CHECK_EQ(Serializer::encode_text_slice(slice), "(\"world\", 6, 11)");

    // 3. Array & Map encoding
    CHECK_EQ(Serializer::encode_state_set(15, 0), "[15, 0]");

    std::vector<Reference> refs = {
        {.sender = ":1.1", .path = "/path/1"},
        {.sender = ":1.1", .path = "/path/2"}
    };
    std::string ref_list = Serializer::encode_reference_list(refs);
    CHECK(ref_list.find("/path/1") != std::string::npos);
    CHECK(ref_list.find("/path/2") != std::string::npos);

    // 4. Signal serialization
    Signal sig{
        .interface_name = "org.a11y.atspi.Event.Object",
        .member = "StateChanged",
        .path = "/org/a11y/atspi/accessible/1",
        .detail = "focused",
        .detail1 = 1,
        .detail2 = 0,
        .any_data = ""
    };
    std::string sig_str = Serializer::serialize_signal(sig);
    CHECK(sig_str.find("SIGNAL org.a11y.atspi.Event.Object.StateChanged") != std::string::npos);
    CHECK(sig_str.find("detail=\"focused\"") != std::string::npos);
    CHECK(sig_str.find("detail1=1") != std::string::npos);

    // 5. Reply serialization
    MethodReply reply{
        .success = true,
        .signature = "s",
        .values = {"\"accessible_name\""},
        .error_message = ""
    };
    std::string rep_str = Serializer::serialize_reply(reply);
    CHECK_EQ(rep_str, "REPLY (s) [\"accessible_name\"]");

    MethodReply err_reply{
        .success = false,
        .signature = "",
        .values = {},
        .error_message = "Object not found"
    };
    CHECK_EQ(Serializer::serialize_reply(err_reply), "ERROR: Object not found");

    return check::finish("test_atspi_serializer");
}
