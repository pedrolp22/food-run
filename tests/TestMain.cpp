#include <exception>
#include <iostream>
#include <string_view>
void mathTests();
void menuIconTests();
void gameplayTests();
void assetTests();
void settingsTests();
int main(int argc, char** argv) {
    try {
        const std::string_view suite = argc > 1 ? argv[1] : "all";
        if (suite == "all" || suite == "math") {
            mathTests();
            menuIconTests();
        }
        if (suite == "all" || suite == "gameplay") {
            gameplayTests();
        }
        if (suite == "all" || suite == "assets") {
            assetTests();
        }
        if (suite == "all" || suite == "settings") {
            settingsTests();
        }
        std::cout << suite << " tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
