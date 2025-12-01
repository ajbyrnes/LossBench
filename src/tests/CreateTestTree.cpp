#include <TFile.h>
#include <TTree.h>

#include <vector>

int main() {
    TFile file("test.root", "RECREATE");
    TTree tree("test", "Test Tree");

    std::vector<float> branch;
    tree.Branch("branch", &branch);

    const std::vector<std::vector<float>> entries = {
        {1.f, 2.f, 3.f, 4.f, 5.f},
        {6.f, 7.f, 8.f},
        {9.f, 10.f}
    };

    for (const auto& entry : entries) {
        branch = entry;
        tree.Fill();
    }

    file.Write();
    file.Close();
    return 0;
}
