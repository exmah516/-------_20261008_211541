#include <zenoh.hxx>
int main() { auto config = zenoh::Config::create_default(); zenoh::KeyExpr key("vi/ads/state/v1"); return 0; }
