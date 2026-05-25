#include "CommonData.slang"

[shader("pixel")]
FSOutput fs_main(v2f input) {
    FSOutput output;

    output.FragColor = float4(push.user.color);

    return output;
}