/*
    usage/main.cpp
    Q@hackers.pk
 */

#include "./main.hh"

int main(int argc, char* argv[]) {
    
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <INPUT filename> <OUTPUT filename>" << std::endl;
        return 1;
    }

    TABLES* tables = nullptr;
    std::string inputFilename = argv[1];

    try
    {
        Parser parser(inputFilename);

        tables = parser.build_hash_table();
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << e.what() << std::endl; 

        return -1;      
    }

    std::ofstream ofile;
    ofile.open(argv[2]);

    // Debug tables...
    for (size_t i = 0; i < tables->get_bucket_used(); i++)
    {
        std::cout<< "i = " << i << ", " << tables->word_id_to_hash[i] << " -> " << tables->hash_to_word_record[tables->word_id_to_hash[i]]->word;

        ofile<< tables->hash_to_word_record[tables->word_id_to_hash[i]]->word << " ";
    }
        
    return 0;
}

