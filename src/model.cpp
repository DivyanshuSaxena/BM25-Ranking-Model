// Run the queries over the given Index and rank the results
// Author: Divyanshu Saxena

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "argparse.hpp"

using namespace std;
using namespace boost;

// FIXME: Weights corresponding to different sections of the topic document.
const int weights[7] = {1 /* domain_weight */,
                        1 /* title_weight */,
                        1 /* desc_weight */,
                        1 /* summary_weight */,
                        1 /* narrative_weight */,
                        1 /* concept_weight */,
                        1 /* def_weight */};

class RunQueries {
 public:
  // Vector to hold the list of terms in the query alongwith the weight
  // corresponding to it.
  // The map is a mapping from string to (weight, frequency) pair.
  vector<map<string, pair<int, int>>> queries;

  // Vector of stopwords that are to be excluded from queries.
  vector<string> stopwords;

  // Map to hold the inverted_index
  map<string, map<string, int>> inverted_index;

  // Map of documents and document length.
  map<string, int> doc_length;

  // Vector to hold results for each of the queries
  vector<vector<pair<string, double>>> results;

  RunQueries() {
    // Initialize queries
    for (int it = 0; it < 50; it++) {
      map<string, pair<int, int>> temp_map;
      queries.push_back(temp_map);
    }

    // Initialize stopwords vector
    ifstream stopwords_file("stopwords");
    string line;
    while (getline(stopwords_file, line)) {
      stopwords.emplace_back(line);
    }
    stopwords_file.close();
    cout << "In Constructor " << stopwords.size() << endl;
  }

  // Get Inputs from Parser class
  void getInputs(map<string, int> dl, map<string, map<string, int>> ii) {
    doc_length = dl;
    inverted_index = ii;
  }

  // Add a single term to query with the associated weight
  int addToQuery(string term, int tag_type, int topic_no) {
    // Convert term to lower case
    to_lower(term);

    // If a stop word is found, ignore it.
    if (find(stopwords.begin(), stopwords.end(), term) != stopwords.end() ||
        term.length() == 1) {
      return 0;
    }

    // Search if term has been added to query
    auto search = queries.at(topic_no - 51).find(term);
    if (search != queries.at(topic_no - 51).end()) {
      // Update with the greater weight.
      search->second.first = max(search->second.first, weights[tag_type]);
      // Update query frequency.
      search->second.second += 1;
    } else {
      // Add term to query.
      queries.at(topic_no - 51).emplace(term, make_pair(weights[tag_type], 1));
    }
    return 0;
  }

  // Get the ranked documents based on weights evaluated from Index Terms and
  // Query Terms.
  void rankDocuments() {
    // Total number of documents
    int N = doc_length.size();

    // Average document length
    double dl_avg = 0;
    for (auto p = doc_length.begin(); p != doc_length.end(); p++) {
      dl_avg += p->second;
    }
    dl_avg = dl_avg / N;

    // FIXME: Parameters k and b for BM25 ranking model
    double k = 1.2;
    double b = 0.5;

    for (int topic = 0; topic < 50; topic++) {
      map<string, double> results_map;

      // Evaluate term weights for all terms in the query
      for (auto it = queries.at(topic).begin(); it != queries.at(topic).end();
           it++) {
        string term = it->first;
        // Evaluate query frequency for term
        int query_frequency = it->second.first * it->second.second;

        // Evaluate weight term (~IDF)
        double df = 0;
        auto term_search = inverted_index.find(term);
        if (term_search != inverted_index.end()) {
          df = term_search->second.size();
        }
        double weight = log((N - df) / df);

        // Evaluate term frequency
        if (term_search != inverted_index.end()) {
          // Term has been found - Weight non zero
          for (auto td = term_search->second.begin();
               td != term_search->second.end(); td++) {
            // Calculate total weight for given pair of term-document.
            string docno = td->first;
            int tf = td->second;
            // Assuming the length of document is available.
            int dl = doc_length.find(docno)->second;
            double term_frequency =
                tf * (1 + k) / (k * (1 - b + (dl / dl_avg) * b) + tf);
            double term_weight = query_frequency * term_frequency * weight;

            // Add the weight in the ranking score of the document.
            auto doc_search = results_map.find(docno);
            if (doc_search != results_map.end()) {
              doc_search->second += term_weight;
            } else {
              results_map.emplace(docno, term_weight);
            }
          }
        }
      }

      // Rank documents based on score
      vector<pair<string, double>> topic_results;
      for (auto score = results_map.begin(); score != results_map.end();
           score++) {
        topic_results.push_back(make_pair(score->first, score->second));
      }
      cout << "Relevant documents for topic " << topic << " "
           << topic_results.size() << endl;

      // Sort the vector of documents along with scores.
      sort(topic_results.begin(), topic_results.end(),
           [](auto& p1, auto& p2) { return (p1.second > p2.second); });

      // Create the final results array
      vector<pair<string, double>> trimmed(topic_results.begin(),
                                           topic_results.end());
      results.push_back(trimmed);
    }
  }

  // Give the output of the queries into the given output file, in a format
  // recognizable by trec_eval.
  void writeToFile(string results_file) {
    ofstream outfile(results_file);
    for (int topic = 51; topic <= 100; topic++) {
      for (int rank = 1; rank <= 50; rank++) {
        string docno = results.at(topic - 51).at(rank - 1).first;
        double score = results.at(topic - 51).at(rank - 1).second;
        outfile << topic << "\tQ0\t" << docno << "\t" << rank << "\t" << score
                << "\tSTANDARD" << endl;
      }
    }
  }
};

class ParseInputs {
 public:
  // Object to create the queries that shall be used for Document Retrieval
  // tasks later.
  std::shared_ptr<RunQueries> rq;

  // Ctor
  ParseInputs(std::shared_ptr<RunQueries> rq) {
    cout << "In ParseInputs" << endl;
    this->rq = rq;
  }

  // Read and parse Index File to generate the Inverted Index.
  void parseIndex(string filename) {
    ifstream index_file(filename);

    // Read document lengths
    string first_line;
    getline(index_file, first_line);
    vector<string> lengths;
    split(lengths, first_line, is_any_of(","));

    map<string, int> doc_length;
    for (int i = 0; i < lengths.size() - 1; i += 2) {
      doc_length.emplace(lengths.at(i), stoi(lengths.at(i + 1)));
    }

    // Read Inverted Index
    string line;
    map<string, map<string, int>> inverted_index;
    while (getline(index_file, line)) {
      vector<string> result;
      split(result, line, is_any_of(","));
      string term = result.at(0);
      map<string, int> term_document_map;
      for (int i = 1; i < result.size() - 1; i += 2) {
        term_document_map.emplace(result.at(i), stoi(result.at(i + 1)));
      }
      inverted_index.emplace(term, term_document_map);
    }

    // Add to RunQueries object
    rq->getInputs(doc_length, inverted_index);
  }

  // Read and parse further after the first line of each tag. Return if two
  // blank lines appear.
  void parseFurther(ifstream& open_file, int topic_no, int tag_type) {
    int num_blank_lines = 0;
    string line;
    while (getline(open_file, line)) {
      char_separator<char> sep(",;:.-!?_'` ()\"\n\t");
      tokenizer<char_separator<char>> tokens(line, sep);

      // Break and return if two blank lines read.
      if (distance(tokens.begin(), tokens.end()) == 0) {
        num_blank_lines++;
        if (num_blank_lines == 2) {
          break;
        }
        continue;
      }

      num_blank_lines = 0;
      for (const auto& t : tokens) {
        rq->addToQuery(t, tag_type, topic_no);
      }
    }
  }

  // Parse the topics file to create query terms.
  void parseTopics(string filename) {
    ifstream topics_file(filename);
    string line;
    int topic_no = 50;
    while (getline(topics_file, line)) {
      char_separator<char> sep(",;:.-!?_'` ()\"\n\t");
      tokenizer<char_separator<char>> tokens(line, sep);

      // Get the first term to dicriminatively analyze tags
      string first_tag;
      for (auto& t : tokens) {
        first_tag = t;
        break;
      }

      // Check with which tag, the first tag matches
      if (first_tag.compare("<dom>") == 0) {
        // cout << "Matched DOM" << endl;  // Debug
        topic_no++;
        int counter = 0;
        for (const auto& t : tokens) {
          counter++;
          if (counter <= 2) continue;
          rq->addToQuery(t, 0, topic_no);
        }
        parseFurther(topics_file, topic_no, 0);
      } else if (first_tag.compare("<title>") == 0) {
        // cout << "Matched TITLE" << endl;  // Debug
        int counter = 0;
        for (const auto& t : tokens) {
          counter++;
          if (counter <= 2) continue;
          rq->addToQuery(t, 1, topic_no);
        }
        parseFurther(topics_file, topic_no, 1);
      } else if (first_tag.compare("<desc>") == 0) {
        // Read Description
        // cout << "Matched DESC" << endl;  // Debug
        parseFurther(topics_file, topic_no, 2);
      } else if (first_tag.compare("<smry>") == 0) {
        // Read Summary
        // cout << "Matched SMRY" << endl;  // Debug
        parseFurther(topics_file, topic_no, 3);
      } else if (first_tag.compare("<narr>") == 0) {
        // Read Narrative
        // cout << "Matched NARR" << endl;  // Debug
        parseFurther(topics_file, topic_no, 4);
      } else if (first_tag.compare("<con>") == 0) {
        // Read Concepts
        // cout << "Matched CON" << endl;  // Debug
        parseFurther(topics_file, topic_no, 5);
      } else if (first_tag.compare("<def>") == 0) {
        // Read Definition
        // cout << "Matched CON" << endl;  // Debug
        parseFurther(topics_file, topic_no, 6);
      } else {
        // For all other lines, discard them.
        continue;
      }
    }
  }
};

int main(int argc, char const* argv[]) {
  // Make a new ArgumentParser and parse the Command Line arguments.
  ArgumentParser parser;
  parser.addArgument("--input", 1);
  parser.addArgument("--topic", 1);
  parser.addArgument("--output", 1);

  // parse the command-line arguments - throws if invalid format
  parser.parse(argc, argv);

  // if we get here, the configuration is valid
  string index_file = parser.retrieve<string>("input");
  string topics_file = parser.retrieve<string>("topic");
  string output_file = parser.retrieve<string>("output");

  auto rq = make_shared<RunQueries>();

  ParseInputs pi(rq);
  pi.parseIndex(index_file);
  pi.parseTopics(topics_file);

  rq->rankDocuments();
  rq->writeToFile(output_file);
}