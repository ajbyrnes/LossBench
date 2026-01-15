void create_test_root() {
    TFile *file = new TFile("test.root", "RECREATE");
    TTree *tree = new TTree("tree", "Test tree with vector<float> branch");

    std::vector<float> data;
    tree->Branch("data", &data);

    // Fill 100 entries with some sample data
    for (int i = 0; i < 100; i++) {
        data.clear();
        // Each entry has a varying number of floats
        int nFloats = 10 + (i % 20);
        for (int j = 0; j < nFloats; j++) {
            data.push_back(static_cast<float>(i * 100 + j) * 0.1f);
        }
        tree->Fill();
    }

    tree->Write();
    file->Close();

    std::cout << "Created test.root with 100 entries" << std::endl;
}
