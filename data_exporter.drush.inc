<?php

/**
 * @file
 *   drush integration.
 */

/**
 * Implements hook_drush_command().
 *
 * @return
 *   An associative array describing your command(s).
 */
function data_exporter_drush_command() {
  $items = array();

  $items['data-export-nodes'] = array(
    'description' => "Export nodes.",
    'callback' => '_data_exporter_export_nodes',
    'options' => array(
      'live-url' => "Live URL to prepend to file paths",
    ),
  );

  return $items;
}

/**
 * Export node data.
 */
function _data_exporter_export_nodes() {
  // Base node query.
  $query = db_select('node', 'n');
  $query->fields('n');
  $query->groupBy('n.nid');

  // Add field data to query.
  if ($field_info_fields = field_info_fields()) {
    foreach ($field_info_fields as $field_info_field) {
      _data_exporter_export_nodes_join_field($field_info_field, $query);
    }
  }

  $result = $query->execute();

  // Output as CSV.
  $out = fopen('php://output', 'w');
  while ($row = $result->fetchAssoc()) {
    fputcsv($out, $row);
  }
  fclose($out);
}

/**
 * Add join for provided field.
 *
 * @param unknown $field_info_field
 * @param unknown $query
 */
function _data_exporter_export_nodes_join_field($field_info_field, $query) {
  if (!isset($field_info_field['storage']['details']['sql']['FIELD_LOAD_CURRENT'])) {
    return;
  }

  $table_details = $field_info_field['storage']['details']['sql']['FIELD_LOAD_CURRENT'];
  $table_name = key($table_details);
  $table_columns = current($table_details);

  $query->leftJoin($table_name, $table_name, 'n.nid = ' . $table_name . '.entity_id AND ' . $table_name . '.entity_type = \'node\'');

  switch ($field_info_field['type']) {
    case 'taxonomy_term_reference':
      $query->leftJoin('taxonomy_term_data', $table_name . '_taxonomy_term_data', $table_name . '_taxonomy_term_data.tid = ' . $table_name . '.' . $table_columns['tid']);
      $query->addExpression('GROUP_CONCAT(DISTINCT ' . $table_name . '_taxonomy_term_data.name)', $table_name . '_terms');
      break;
    case 'file':
    case 'image':
      $base_path = variable_get('file_public_path', conf_path() . '/files') . '/';
      if ($live_url = drush_get_option('live-url')) {
        $base_path = $live_url . '/' . $base_path;
      }
      $query->leftJoin('file_managed', $table_name . '_file_managed', $table_name . '_file_managed.fid = ' . $table_name . '.' . $table_columns['fid']);
      $query->addExpression('REPLACE(' . $table_name . '_file_managed.uri, :public, :base_url)', $table_name . '_url', array(
        ':public' => 'public://',
        ':base_url' => $base_path,
      ));
      break;
    case 'user_reference':
    case 'text':
    case 'datetime':
    case 'text_long':
    case 'text_with_summary':
    case 'list_boolean':
    case 'list_text':
    case 'node_reference':
    default:
      // Select all field columns as they are.
      foreach ($table_columns as $column_key => $column_name) {
        $query->addField($table_name, $column_name, $table_name . '_' . $column_name);
      }
      break;
  }
}
