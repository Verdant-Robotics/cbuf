cc_binary(
    name="vrm_test",
    srcs=[
        "test_main.cpp",
        ":image_header",
    ],
    deps=[
        "//:vrm_headers",
    ],
    includes = ["."],
)

genrule(
    name="image_header",
    srcs=[
        "samples/image.vrm",
    ],
    outs = [
        "image.h",
    ],
    tools = [
        "//:vrm",
    ],
    cmd = "$(location //:vrm) $(location samples/image.vrm) > $@",
)