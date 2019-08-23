// Create the Inverted Index from the documents in the Input Directory.
// Author: Divyanshu Saxena

#include <dirent.h>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "argparse.hpp"
#include "pugixml.hpp"

using pugi::xml_document;
using pugi::xml_node;
using pugi::xml_parse_result;
using namespace std;
using namespace boost;

class CreateIndex {
 public:
  map<string, map<string, int>> inverted_index;

  // Vector of stopwords that are to be excluded from queries.
  vector<string> stopwords;

  // Current docno and number of words found till now in the current doc.
  string curr_docno;
  int num_words;

  // Map of documents and document length, to be written in Index File.
  map<string, int> doc_length;

  // Ctor
  CreateIndex() {
    // Initialize curr_docno and num_words
    curr_docno = "";
    num_words = 0;

    // Initialize stopwords vector.
    ifstream stopwords_file("stopwords");
    string line;
    while (getline(stopwords_file, line)) {
      stopwords.emplace_back(line);
    }
    stopwords_file.close();
    cout << "In Constructor " << stopwords.size() << endl;
  }

  // Tokenize words from text and add them to the Inverted Index List.
  void addToIndex(string text, string docno) {
    // Tokenize text to get final index terms
    char_separator<char> sep(",;.-!?_'` ()\"");
    tokenizer<char_separator<char>> tokens(text, sep);
    for (const auto &t : tokens) {
      // Check if term is not a stopword
      if (find(stopwords.begin(), stopwords.end(), t) != stopwords.end() ||
          t.length() == 1) {
        continue;
      }

      if (curr_docno.compare(docno) == 0) {
        num_words += 1;
      } else {
        if (curr_docno != "") {
          doc_length.emplace(curr_docno, num_words);
        }
        curr_docno = docno;
        num_words = 1;
      }

      // Add term in Inverted Index, if not already added.
      auto search = inverted_index.find(t);
      if (search != inverted_index.end()) {
        // Search if this document is present.
        auto doc_search = search->second.find(docno);
        if (doc_search != search->second.end()) {
          doc_search->second += 1;
        } else {
          search->second.emplace(docno, 1);
        }
      } else {
        map<string, int> term_document_map;
        term_document_map.emplace(docno, 1);
        inverted_index.emplace(t, term_document_map);
      }
    }
  }

  // Parse all the documents in the input_dir directory and add them to the
  // index.
  void parseDocuments(string input_dir) {
    // Open directory to read its contents
    DIR *dirp = opendir(input_dir.c_str());
    struct dirent *dp;
    while ((dp = readdir(dirp)) != NULL) {
      if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
        continue;
      }

      // Edit document to make a valid DOC
      string filename = input_dir + "/" + dp->d_name;
      // Temporary file to hold the valid XML type document
      string newfile = filename + "_temp";

      ofstream outfile(newfile);
      ifstream infile(filename);

      outfile << "<FILE>" << endl;
      outfile << infile.rdbuf();
      outfile << "</FILE>" << endl;

      infile.close();

      // Open a single SGML document
      xml_document file;
      xml_parse_result result = file.load_file(newfile.c_str());
      cout << "Reading file " << filename << endl;  // Debug
      if (!result) {
        cout << "Could not capture Directory Structure" << endl;
      }

      // Iterate over all documents in a single SGML document
      for (xml_node doc : file.child("FILE").children("DOC")) {
        string docno = doc.child_value("DOCNO");
        trim(docno);
        for (xml_node doc_text = doc.child("TEXT"); doc_text;
             doc_text = doc_text.next_sibling("TEXT")) {
          for (xml_node tag = doc_text.first_child(); tag;
               tag = tag.next_sibling()) {
            // Currently, treating all children as equivalent.
            // TODO: Use the tags for relevant information.
            string text = tag.text().get();
            addToIndex(to_lower_copy(text), docno);
          }
        }
      }

      outfile.close();
      remove(newfile.c_str());
    }
    closedir(dirp);
    cout << inverted_index.size() << endl;
  }

  // Save the Inverted Index into a File as Comma Separated Values.
  void saveToFile(string filename) {
    ofstream indexfile(filename);

    // Save the document lengths, so as to retrieve them while ranking results.
    for (auto it = doc_length.begin(); it != doc_length.end(); it++) {
      indexfile << it->first << ", " << it->second << ", ";
    }
    indexfile << endl;

    // Save the inverted index
    for (auto it = inverted_index.begin(); it != inverted_index.end(); it++) {
      indexfile << it->first << ", ";
      // Print the pair of docno, frequency
      for (auto doc_pair = it->second.begin(); doc_pair != it->second.end();
           doc_pair++) {
        indexfile << doc_pair->first << ", " << doc_pair->second << ", ";
      }
      indexfile << endl;
    }
  }
};

int main(int argc, char const *argv[]) {
  // Make a new ArgumentParser and parse the Command Line arguments.
  ArgumentParser parser;
  parser.addArgument("--input", 1);
  parser.addArgument("--output", 1);

  // parse the command-line arguments - throws if invalid format
  parser.parse(argc, argv);

  // if we get here, the configuration is valid
  string input_dir = parser.retrieve<string>("input");
  string output_file = parser.retrieve<string>("output");

  CreateIndex ci;
  ci.parseDocuments(input_dir);
  ci.saveToFile(output_file);
}