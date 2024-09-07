


    // !!! This file is generated using emlearn !!!

    #include <eml_trees.h>
    

static const EmlTreesNode seed_classifier_nodes[21] = {
  { 1, 107, 1, 20 },
  { 6, 20, 1, -1 },
  { 6, 19, 1, -2 },
  { 6, 13, 1, 12 },
  { 6, 9, 1, 8 },
  { 6, 8, 1, -3 },
  { 6, 5, 1, 5 },
  { 6, 4, 1, -4 },
  { 0, 70, 1, -5 },
  { 6, 2, -6, 1 },
  { 4, 55, -7, -8 },
  { 6, 7, -9, -10 },
  { 6, 10, -11, 1 },
  { 6, 11, -12, 1 },
  { 6, 12, -13, -14 },
  { 0, 75, 1, -15 },
  { 6, 16, 1, 2 },
  { 1, 32, -16, -17 },
  { 6, 17, -18, 1 },
  { 4, 77, -19, -20 },
  { 4, 87, -21, -22 } 
};

static const int32_t seed_classifier_tree_roots[1] = { 0 };

static const uint8_t seed_classifier_leaves[22] = { 21, 20, 9, 5, 1, 2, 3, 4, 6, 8, 10, 11, 12, 13, 15, 16, 14, 17, 18, 19, 7, 0 };

EmlTrees seed_classifier = {
        21,
        (EmlTreesNode *)(seed_classifier_nodes),	  
        1,
        (int32_t *)(seed_classifier_tree_roots),
        22,
        (uint8_t *)(seed_classifier_leaves),
        0,
        7,
        22,
    };

static inline int32_t seed_classifier_tree_0(const int16_t *features, int32_t features_length) {
          if (features[1] < 107) {
              if (features[6] < 20) {
                  if (features[6] < 19) {
                      if (features[6] < 13) {
                          if (features[6] < 9) {
                              if (features[6] < 8) {
                                  if (features[6] < 5) {
                                      if (features[6] < 4) {
                                          if (features[0] < 70) {
                                              if (features[6] < 2) {
                                                  return 2;
                                              } else {
                                                  if (features[4] < 55) {
                                                      return 3;
                                                  } else {
                                                      return 4;
                                                  }
                                              }
                                          } else {
                                              return 1;
                                          }
                                      } else {
                                          return 5;
                                      }
                                  } else {
                                      if (features[6] < 7) {
                                          return 6;
                                      } else {
                                          return 8;
                                      }
                                  }
                              } else {
                                  return 9;
                              }
                          } else {
                              if (features[6] < 10) {
                                  return 10;
                              } else {
                                  if (features[6] < 11) {
                                      return 11;
                                  } else {
                                      if (features[6] < 12) {
                                          return 12;
                                      } else {
                                          return 13;
                                      }
                                  }
                              }
                          }
                      } else {
                          if (features[0] < 75) {
                              if (features[6] < 16) {
                                  if (features[1] < 32) {
                                      return 16;
                                  } else {
                                      return 14;
                                  }
                              } else {
                                  if (features[6] < 17) {
                                      return 17;
                                  } else {
                                      if (features[4] < 77) {
                                          return 18;
                                      } else {
                                          return 19;
                                      }
                                  }
                              }
                          } else {
                              return 15;
                          }
                      }
                  } else {
                      return 20;
                  }
              } else {
                  return 21;
              }
          } else {
              if (features[4] < 87) {
                  return 7;
              } else {
                  return 0;
              }
          }
        }
        

int32_t seed_classifier_predict(const int16_t *features, int32_t features_length) {

        int32_t votes[22] = {0,};
        int32_t _class = -1;

        _class = seed_classifier_tree_0(features, features_length); votes[_class] += 1;
    
        int32_t most_voted_class = -1;
        int32_t most_voted_votes = 0;
        for (int32_t i=0; i<22; i++) {

            if (votes[i] > most_voted_votes) {
                most_voted_class = i;
                most_voted_votes = votes[i];
            }
        }
        return most_voted_class;
    }
    