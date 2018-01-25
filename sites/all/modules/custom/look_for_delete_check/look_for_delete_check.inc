<?php

/**
 * implement confirm form().
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @return $form
 *
 */
function look_for_delete_check_confirm_form($form, &$form_state) {
  $path = 'admin/configuration/delete-contents';
  $form = confirm_form($form,
    t('Are you sure, you want to update?'),
    $path,
    t('This action cannot be undone.<br><br>'),
    t('Confirm'),
    t('Cancel')
  );
  $form['csv_link'] = array(
    '#type' => 'submit',
    '#value' => 'Download list',
    '#submit' => array('look_for_delete_check_csv'),
  );
  $default_content = $form_state['input']['select_content_type'];
  $currentfields = $form_state['input']['nodes'];
  if (!empty(array_filter($form_state['input']['userdelete']))) {
    $delete_user = $form_state['input']['userdelete'];
    $form['user_delete'] = array(
      '#type' => 'hidden',
      '#value' => $delete_user
    );
  }
  if ((isset($form_state['input']['nodes']['update'])) || (isset($form_state['input']['userdelete']))) {
    $current_option =array_filter($form_state['input']['nodes']['update']);
    if (!empty($current_option)) {
      $serialised_update_options = serialize($current_option);
      $form['update_options'] = array(
        '#type' => 'hidden',
        '#value' => $serialised_update_options
      );
    }
    if (!empty($form_state['input']['range'])) {
      $range = $form_state['input']['range'];
    }
    $state = 0;
    foreach ($currentfields as $key1 => $value1) {
      if (!empty($value1['search_name'])) {
        $state = 1;
      }
    }
    if ($state == 1) {
      $query = db_select('node','n')->distinct();
      $query->fields('n',array('nid','uid'));
      if (!empty($range)) {
        $query->range(0,$range);
      }
      $query->condition('n.type', $default_content);
      $form = look_for_delete_check_query($form, $form_state, $currentfields,$default_content, $query);
    }
  }
  $form['actions']['submit']['#submit'][] = 'look_for_delete_check_confirm_form_submit';
  return $form;
}

/**
 * Function for query part.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $form.
 */
function look_for_delete_check_query($form, &$form_state, $currentfields, $default_content, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      $keyword = explode(',', $field_data['search_name']);
      if ($field_name == 'title') {
        $query = look_for_delete_check_field_title($form, $form_state, $default_content, $currentfields, $keyword, $query);
      }
      else {
        $field_types = field_info_field($field_name);
        $current_fieldtype = $field_types['type'];
        $query->leftjoin('field_data_' . $field_name, $field_name, 'n.nid = ' . $field_name . '.entity_id');
        if (($current_fieldtype == 'image') || ($current_fieldtype == 'taxonomy_term_reference') || ($current_fieldtype == 'paragraphs')) {
          if ($current_fieldtype == 'image') {
            $query = look_for_delete_check_field_image($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
          }
          elseif ($current_fieldtype == 'taxonomy_term_reference') {
            $query = look_for_delete_check_field_taxonomy_term_reference($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
          }
          elseif ($current_fieldtype == 'paragraphs') {
            $query = look_for_delete_check_field_paragraphs($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
          }
        }
        else {
          if ($current_fieldtype == 'number_integer') {
            if (($field_data['operator'] == '>') || ($field_data['operator'] == '>=') || ($field_data['operator'] == '<') || ($field_data['operator'] == '<=')) {
              if (is_array($keyword)) {
                $query_and = db_and();
                foreach ($keyword as $keyword_value) {
                  $query_and->condition($field_name . '.' . $field_name . '_value', $keyword_value, $field_data['operator']);
                }
                $query->condition($query_and);
              }
              else {
                $query->condition($field_name . '.' . $field_name . '_value', $keyword_value, $field_data['operator']);
              }
            }
            else {
              $query = look_for_delete_check_MULTIPLE_FIELD_TYPES($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
            }
          }
          else {
            $query = look_for_delete_check_MULTIPLE_FIELD_TYPES($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
          }
        }
      }
    }
  }
  $nids = $query->execute()->fetchAll();
  if (!empty($nids)) {
    $serialised_nids = serialize($nids);
    $form['nid_values'] = array(
      '#type' => 'hidden',
      '#value' => $serialised_nids
    );
  }
  else {
    drupal_set_message('No content satisfies the given conditions');
  }
  return $form;
}

/**
 * Function for image search.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $current_fieldtype
 *  User selected field type.
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $query.
 */
function look_for_delete_check_field_image($form, &$form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      $query->leftjoin('field_data_' . $field_name, $field_name, 'n.nid = ' . $field_name . '.entity_id');
      $query->leftjoin( 'file_managed', 'fm', $field_name . '.' . $field_name. '_fid = fm.fid');
      if (is_array($keyword)) {
        if (($field_data['operator'] == '=') || ($field_data['operator'] == 'LIKE')) {
          $query_or = db_or();
          foreach ($keyword as $keyword_value) {
            if ($field_data['operator'] == '=') {
              $query_or->condition('fm.filename', $keyword_value, $field_data['operator']);
            }
            else {
              $query_or->condition('fm.filename', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
          }
          $query->condition($query_or);
        }
        else {
          $query_and = db_and();
          foreach ($keyword as $keyword_value) {
            if ($field_data['operator'] == '!=') {
              $query_and->condition('fm.filename', $keyword_value, $field_data['operator']);
            }
            else {
              $query_and->condition('fm.filename', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
          }
          $query->condition($query_and);
        }
      }
      else {
        $query->condition('fm.filename', $keyword_value, $field_data['operator']);
      }
    }
  }
  return $query;
}

/**
 * Function for paragraphs search.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $current_fieldtype
 *  User selected field type.
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $query.
 */
function look_for_delete_check_field_paragraphs($form, &$form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      $query->leftjoin('field_data_' . $field_name, $field_name, 'n.nid = ' . $field_name . '.entity_id');
      $query->leftjoin( 'paragraphs_item', 'pi', $field_name . '.' . $field_name . '_value = pi.item_id');
      $query->leftjoin( 'paragraphs_bundle', 'pb', 'pi.bundle = pb.bundle');
      if (is_array($keyword)) {
        if (($field_data['operator'] == '=') || ($field_data['operator'] == 'LIKE')) {
          $query_or = db_or();
          foreach ($keyword as $keyword_value) {
            if ($field_data['operator'] == '=') {
              $query_or->condition('pb.name', $keyword_value, $field_data['operator']);
            }
            else {
              $query_or->condition('pb.name', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
            $query->condition($query_or);
          }
        }
        else {
          $query_and = db_and();
          foreach ($keyword as $keyword_value) {
            if ($field_data['operator'] == '!=') {
              $query_and->condition('pb.name', $keyword_value, $field_data['operator']);
            }
            else {
              $query_and->condition('pb.name', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
          }
          $query->condition($query_and);
        }
      }
      else {
        $query->condition('pb.name', $keyword_value, $field_data['operator']);
      }
    }
  }
  return $query;
}

/**
 * Function for image search.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $current_fieldtype
 *  User selected field type.
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $query.
 */
function look_for_delete_check_field_taxonomy_term_reference($form, &$form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      $query->leftjoin('field_data_' . $field_name, $field_name, 'n.nid = ' . $field_name . '.entity_id');
      $query->leftjoin( 'taxonomy_term_data', 'ttd', $field_name . '.' . $field_name. '_tid = ttd.tid');
      if (is_array($keyword)) {
        if (($field_data['operator'] == '=') || ($field_data['operator'] == 'LIKE')) {
          $query_or = db_or();
          foreach ($keyword as $keyword_value) {
            if ($field_data['operator'] == '=') {
              $query_or->condition('ttd.name', $keyword_value, $field_data['operator']);
            }
            else {
              $query_or->condition('ttd.name', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
            $query->condition($query_or);
          }
        }
        else {
          $query_and = db_and();
          foreach ($keyword as $keyword_value) {
            if ($field_data['operator'] == '!=') {
              $query_and->condition('ttd.name', $keyword_value, $field_data['operator']);
            }
            else {
              $query_and->condition('ttd.name', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
          }
          $query->condition($query_and);
        }
      }
      else {
        $query->condition('ttd.name', $keyword_value, $field_data['operator']);
      }
    }
  }
  return $query;
}

/**
 * Function for title search.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $query.
 */
function look_for_delete_check_field_title($form, $form_state, $default_content, $currentfields, $keyword, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      if (($field_data['operator'] == '=') || ($field_data['operator'] == '!=')) {
        if (is_array($keyword)) {
          if ($field_data['operator'] == '=') {
            $query_or = db_or();
            foreach ($keyword as $keyword_value) {
              $query_or->condition('n.title', $keyword_value, $field_data['operator']);
            }
            $query->condition($query_or);
          }
          elseif ($field_data['operator'] == '!=') {
            $query_and = db_and();
            foreach ($keyword as $keyword_value) {
              $query_and->condition('n.title', $keyword_value, $field_data['operator']);
            }
            $query->condition($query_and);
          }
        }
        else {
          $query->condition('n.title', $keyword_value, $field_data['operator']);
        }
      }
      else {
        if (is_array($keyword)) {
          if ($field_data['operator'] == 'LIKE') {
            $query_or = db_or();
            foreach ($keyword as $keyword_value) {
              $query_or->condition('n.title', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
            $query->condition($query_or);
          }
          else {
            $query_and = db_and();
            foreach ($keyword as $keyword_value) {
              $query_and->condition('n.title', '%' . db_like($keyword_value) . '%', $field_data['operator']);
            }
            $query->condition($query_and);
          }
        }
        else {
          $query->condition('n.title', '%' . db_like($keyword_value) . '%', $field_data['operator']);
        }
      }
    }
  }
  return $query;
}

/**
 * Search for field type 'text', 'telephone', 'textwith summery', 'email', 'link_field' & 'number_intiger'.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $current_fieldtype
 *  User selected field type.
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $query.
 */
function look_for_delete_check_MULTIPLE_FIELD_TYPES($form, &$form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      $query->leftjoin('field_data_' . $field_name, $field_name, 'n.nid = ' . $field_name . '.entity_id');
      if (is_array($keyword)) {
        if ($field_data['operator'] == 'LIKE') {
          $query_or = db_or();
          foreach ($keyword as $keyword_value) {
            if ($current_fieldtype == 'link_field') {
              $query_or->condition($field_name . '.' . $field_name . '_url','%' .db_like($keyword_value) . '%', $field_data['operator']);
            }
            elseif ($current_fieldtype == 'email') {
              $query_or->condition($field_name . '.' . $field_name . '_email','%' .db_like($keyword_value) . '%', $field_data['operator']);
            }
            else {
              $query_or->condition($field_name . '.' . $field_name . '_value','%' .db_like($keyword_value) . '%', $field_data['operator']);
            }
            $query->condition($query_or);
          }
        }
        elseif ($field_data['operator'] == '=') {
          $query_or = db_or();
          foreach ($keyword as $keyword_value) {
            if ($current_fieldtype == 'link_field') {
              $query_or->condition($field_name . '.' . $field_name . '_url',$keyword_value, $field_data['operator']);
            }
            elseif ($current_fieldtype == 'email') {
              $query_or->condition($field_name . '.' . $field_name . '_email',$keyword_value, $field_data['operator']);
            }
            else {
              $query_or->condition($field_name . '.' . $field_name . '_value',$keyword_value, $field_data['operator']);
            }
          }
          $query->condition($query_or);
        }
        else {
          $query = look_for_delete_check_subquery_operators($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
        }
      }
      else {
        $query = look_for_delete_check_NO_ARRAY_ELSE_QUERY($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query);
      }
    }
  }
  return $query;
}

/**
 * Function for search in which subquery is using.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $current_fieldtype
 *  User selected field type.
 *
 * @param $query
 *  It have the query conditions.
 *
 * @return $query.
 */
function look_for_delete_check_subquery_operators($form, &$form_state, $default_content, $currentfields, $keyword, $current_fieldtype, $query) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      $query->leftjoin('field_data_' . $field_name, $field_name, 'n.nid = ' . $field_name . '.entity_id');
      if (is_array($keyword)) {
        $query_and = db_and();
        if ($field_data['operator'] == '!=') {
          foreach ($keyword as $keyword_value) {
            $subquery = db_select('field_data_' . $field_name, $field_name);
            $subquery->fields($field_name, array('entity_id'));
            if ($current_fieldtype == 'link_field') {
              $subquery->condition($field_name . '.' . $field_name . '_url',$keyword_value, 'LIKE');
            }
            elseif ($current_fieldtype == 'email') {
              $subquery->condition($field_name . '.' . $field_name . '_email',$keyword_value, 'LIKE');
            }
            else {
              $subquery -> condition($field_name . '.' . $field_name . '_value',$keyword_value, 'LIKE');
            }
            $result = $subquery->execute()->fetchCol();
            foreach ($result as $result_value) {
              $query_and->condition('n.nid', $result_value, $field_data['operator']);
            }
            $query->condition($query_and);
          }
        }
        else {
          foreach ($keyword as $keyword_value) {
            $subquery = db_select('field_data_' . $field_name, $field_name);
            $subquery->fields($field_name, array('entity_id'));
            if ($current_fieldtype == 'link_field') {
              $subquery->condition($field_name . '.' . $field_name . '_url','%' .db_like($keyword_value) . '%', 'LIKE');
            }
            elseif ($current_fieldtype == 'email') {
              $subquery->condition($field_name . '.' . $field_name . '_email','%' .db_like($keyword_value) . '%', 'LIKE');
            }
            else {
              $subquery -> condition($field_name . '.' . $field_name . '_value','%' .db_like($keyword_value) . '%', 'LIKE');
            }
            $result = $subquery->execute()->fetchCol();
            foreach ($result as $result_value) {
              $query_and->condition('n.nid', $result_value, $field_data['operator']);
            }
            $query->condition($query_and);
          }
        }
      }
      else {
        $query = look_for_delete_check_NO_ARRAY_ELSE_QUERY($form, $form_state, $default_content, $currentfields, $keyword, $current_fieldtype);
      }
    }
  }
  return $query;
}

/**
 * Function for query if only one file name is given.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 * @param $default_content
 *  The content type chosen by user.
 *
 * @param $currentfields
 *  It contains the data given by user.
 *
 * @param $keyword
 *  User given Desired filename (seperated by coma).
 *
 * @param $current_fieldtype
 *  User selected field type.
 *
 * @return $query.
 */
function look_for_delete_check_NO_ARRAY_ELSE_QUERY($form, &$form_state, $default_content, $currentfields, $current_fieldtype, $keyword) {
  foreach ($currentfields as $field_name => $field_data) {
    if ((isset($field_data['search_name']) && !empty($field_data['search_name'])) && (isset($field_data['operator']) && !empty($field_data['operator']))) {
      if (($field_data['operator'] == '!=') || ($field_data['operator'] == '=')) {
        if ($current_fieldtype == 'link_field') {
          $query->condition($field_name . '.' . $field_name . '_url',$keyword_value, $field_data['operator']);
        }
        elseif ($current_fieldtype == 'email') {
          $query->condition($field_name . '.' . $field_name . '_email',$keyword_value, $field_data['operator']);
        }
        else {
          $query->condition($field_name . '.' . $field_name . '_value',$keyword_value, $field_data['operator']);
        }
      }
      else{
        if ($current_fieldtype == 'link_field') {
          $query->condition($field_name . '.' . $field_name . '_url','%' .db_like($keyword_value) . '%', $field_data['operator']);
        }
        elseif ($current_fieldtype == 'email') {
          $query->condition($field_name . '.' . $field_name . '_email','%' .db_like($keyword_value) . '%', $field_data['operator']);
        }
        else {
          $query->condition($field_name . '.' . $field_name . '_value','%' .db_like($keyword_value) . '%', $field_data['operator']);
        }
      }
    }
  }
  return $query;
}

/**
 * Implements conform form_submit().
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 */
function look_for_delete_check_confirm_form_submit($form, &$form_state) {
  if (isset($form_state['input']['nid_values'])) {
    $needed_nids = $form_state['input']['nid_values'];
    $current_nids = unserialize($needed_nids);
    if (!empty($form_state['input']['update_options'])) {
      $current_options = unserialize($form_state['input']['update_options']);
      $batch = array(
        'title' => t('Processing'),
        'operations' =>array(),
        'init_message' => t('Batch process is started'),
        'progress_message' => t('completed @current out of @total'),
        'finished' => 'look_for_delete_check_batch_finished',
      );
      $progress = 0; // where to start
      $limit = 100; // how many to process for each run
      $max = count($current_nids);
      while ($progress < $max) {
        $current_nid = array_slice($current_nids, $progress, $limit);
        $batch['operations'][] = array('look_for_delete_check_confirm_batch_process_1',array($current_options, $current_nid, $progress, $limit));
        $progress = $progress + $limit;
      }
      batch_set($batch);
    }
    elseif (isset($form_state['input']['user_delete'])) {
      $uids = array();
      $batch = array(
        'title' => t('Processing'),
        'init_message' => t('Batch process is started'),
        'progress_message' => 'completed @current out of @total',
        'operations' => array(),
        'finished' => 'look_for_delete_check_batch_finished',
      );
      $batch['operations'][] = array('look_for_delete_check_confirm_batch_process_2',array($current_nids));
      batch_set($batch);
    }
    else {
      drupal_set_message('Error');
    }
  }
}

/**
 *implement batch process for updation.
 *
 * @param $current_options
 *  Selected update option
 *
 * @param $current_nids
 * Node get as query output
 *
 */
function look_for_delete_check_confirm_batch_process_1($current_options, $current_nid, $progress, $limit) {
  foreach ($current_options as $current_option) {
    if (($current_option == 'unpublish') || ($current_option == 'publish')) {
      foreach ($current_nid as $current_nid_value) {
        $nid = $current_nid_value->nid;
        $nodes = node_load($nid);
        if ($current_option == 'unpublish') {
          $nodes->status = 0;
          node_save($nodes);
        }
        else {
          $nodes->status = 1;
          node_save($nodes);
        }
      }
    }
    else {
      foreach ($current_nid as $current_nid_value) {
        $nid = $current_nid_value->nid;
        node_delete($nid);
      }
    }
  }
}

/**
 * Implement batch process for user delete.
 *
 * @param $current_nids
 *  Node get as query output
 *
 * @param $user_delete
 *  Status of checkbox for delete the user
 *
 */
function look_for_delete_check_confirm_batch_process_2($current_nids) {
  foreach ($current_nids as $uids) {
    $uid = $uids->uid;
    user_delete($uid);
  }
}

/**
 * Function for batch process finished .
 *
 * @param $success
 *  Batch output
 *
 * @param $operations
 *  Node get as query output
 *
 */
function look_for_delete_check_batch_finished($success) {
  if ($success) {
    drupal_set_message('All done');
  }
  else {
    drupal_set_message('Error');
  }
}

/**
 * Function to get csv file.
 *
 * @param $form
 *  Form object
 *
 * @param $form_state
 *  Form state values
 *
 */
function look_for_delete_check_csv($form, &$form_state) {
  $needed_nid_csv = $form_state['input']['nid_values'];
  $current_nid_csv = unserialize($needed_nid_csv);
  if(!empty($current_nid_csv)) {
    //add necessary headers for browsers
     drupal_add_http_header('Content-Disposition', 'attachment; filename = look_for_delete_check_csv_file.csv');
    //instead of writing down to a file we write to the output stream
    $fh = fopen('php://output', 'w');
    //form header
    fputcsv($fh, array(t('NIDS'), t('Node Title'), t('User')));
    //write data in the CSV format
    foreach ($current_nid_csv as $current_nids_values) {
      $csv_nid = $current_nids_values->nid;
      $csv_nodes = node_load($csv_nid);
      $data = array($csv_nodes->nid, $csv_nodes->title, $csv_nodes->name);
      fputcsv($fh, array($csv_nodes->nid, $csv_nodes->title, $csv_nodes->name));
    }
    //close the stream
    fclose($fh);
    drupal_exit();
  }
}
