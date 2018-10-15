<?php

function gtm_admin_page() {
  $cache = cache_get('gtm_admin_data');
  $query_params = drupal_get_query_parameters();
  if (!count($query_params) && !empty($cache) && !empty($cache->data)) {
    $cache_data = $cache->data;
    $loaded = 'from caches';
    $cached = TRUE;
  }
  else {
    $cache_data = array();
    $loaded = 'from API';
    $cached = FALSE;
  }

  $service = _gtm_admin_get_google_gtm_service();
  if (!$service) {
    return '';
  }

  $workspace_changes_ids = array(
    'tags' => array(),
  );
  $output = '';
  $output .= theme('html_tag', array(
    'element' => array(
      '#tag' => 'span',
      '#value' => t('Loaded: ' . $loaded),
    ),
  ));

  if (isset($cache_data['accounts'])) {
    $accounts = $cache_data['accounts'];
  }
  else {
    $accounts = $service->accounts->listAccounts();
    $cache_data['accounts'] = $accounts;
  }

  foreach ($accounts->getAccount() as $account) {
    $account_id = $account->getAccountId();
    $account_h2 = theme('html_tag', array(
      'element' => array(
        '#tag' => 'h2',
        '#value' => $account->getName(),
      ),
    ));
    $account_details = theme('html_tag', array(
      'element' => array(
        '#tag' => 'span',
        '#value' => t('Account ID: ') . $account_id,
      ),
    ));
    $output .= theme('html_tag', array(
      'element' => array(
        '#tag' => 'div',
        '#value' => $account_h2 . $account_details,
      ),
    ));

    if (isset($cache_data[$account_id]['containers'])) {
      $containers = $cache_data[$account_id]['containers'];
    }
    else {
      $params = array('accounts' => $account_id);
      $params_string = _gtm_admin_get_parent_string($params);
      $containers = $service->accounts_containers->listAccountsContainers($params_string);
      $cache_data[$account_id]['containers'] = $containers;
    }

    $header = array(
      'Container',
      'Workspace',
      'Tag',
      'Action',
    );
    $rows = array();

    foreach ($containers->getContainer() as $container) {
      $container_id = $container->getContainerId();
      $container_name = $container->getName() . ' (' . $container->getPublicId() . ')';
      $rows[] = array($container_name, '', '', '');

      if (isset($cache_data[$account_id][$container_id]['workspaces'])) {
        $workspaces = $cache_data[$account_id][$container_id]['workspaces'];
      }
      else {
        $params = array(
          'accounts' => $account->getAccountId(),
          'containers' => $container->getContainerId(),
        );
        $params_string = _gtm_admin_get_parent_string($params);
        $workspaces = $service->accounts_containers_workspaces->listAccountsContainersWorkspaces($params_string);
        $cache_data[$account_id][$container_id]['workspaces'] = $workspaces;
      }

      foreach ($workspaces->getWorkspace() as $workspace) {
        $workspace_id = $workspace->getWorkspaceId();
        $workspace_name = $workspace->getName();
        $params = array(
          'accounts' => $account->getAccountId(),
          'containers' => $container->getContainerId(),
          'workspaces' => $workspace_id,
        );
        $params_string = _gtm_admin_get_parent_string($params);
        try {
          $status_object = $service->accounts_containers_workspaces->getStatus($params_string);
        }
        catch (Google_Service_Exception $e) {
          if ($cached) {
            cache_clear_all('gtm_admin_data', 'cache');
            drupal_set_message('Looks like GTM data was updated. Clearing caches.');
            drupal_goto(GTM_ADMIN_LISTING_PAGE_URL);
          }
          drupal_set_message('Google API error. Check logs for more details.', 'error');
          watchdog_exception('gtm_admin', $e);
          break;
        }

        $workspace_changes = $status_object->getWorkspaceChange();
        $actions = '';
        if (!empty($workspace_changes)) {
          $changes_names = array();
          foreach ($workspace_changes as $changes) {
            $changes_tags = $changes->getTag();
            $changes_names[] = 'Tag: ' . $changes_tags->getName();
            $workspace_changes_ids['tags'][] = $changes_tags->getTagId();
          }
          $changes_names = implode(',', $changes_names);

          $workspace_name = $workspace_name . theme('html_tag', array(
              'element' => array(
                '#tag' => 'b',
                '#value' => ' changed (' . $changes_names . ')',
              ),
            ));
          $actions = l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), array(
            'query' => array(drupal_get_destination()),
            'attributes' => array('style' => 'color: #ff0000; font-size: 150%;'),
          ));
        }

        $actions .= l('Add tag', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + array('tags' => 0)), array(
          'query' => array(drupal_get_destination()),
        ));
        $rows[] = array('', $workspace_name, '', $actions);

        if (isset($cache_data[$account_id][$container_id][$workspace_id]['tags'])) {
          $tags = $cache_data[$account_id][$container_id][$workspace_id]['tags'];
        }
        else {
          $params = array(
            'accounts' => $account->getAccountId(),
            'containers' => $container->getContainerId(),
            'workspaces' => $workspace_id,
          );
          $params_string = _gtm_admin_get_parent_string($params);
          $tags = $service->accounts_containers_workspaces_tags->listAccountsContainersWorkspacesTags($params_string);
          $cache_data[$account_id][$container_id][$workspace_id]['tags'] = $tags;
        }

        foreach ($tags->getTag() as $tag) {
          $tag_id = $tag->getTagId();

          $tag_name = $tag->getName();
          if (in_array($tag_id, $workspace_changes_ids['tags'])) {
            $tag_name .= theme('html_tag', array(
              'element' => array(
                '#tag' => 'b',
                '#value' => ' (new)',
              ),
            ));
          }
          $rows[] = array('', '', $tag_name,
            l('Edit', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + array('tags' => $tag_id)), array(
              'query' => array(drupal_get_destination()),
            ))
            . ' ' .
            l('Delete', GTM_ADMIN_TAG_URL . '/delete/' . _gtm_admin_get_param_string($params + array('tags' => $tag_id)), array(
              'query' => array(drupal_get_destination()),
            )),
        );
        }
      }
    }

    $table = [
      'header' => $header,
      'rows' => $rows,
      'attributes' => [],
      'caption' => '',
      'colgroups' => [],
      'empty' => FALSE,
      'sticky' => FALSE,
    ];
    $output .= theme_table($table);

  }

  //cache_set('gtm_admin_data', $cache_data, 'cache', CACHE_TEMPORARY);

  return $output;
}

function gtm_admin_account_page($account_id) {

  return '';
}

function gtm_admin_container_page($account_id, $container_id) {
  module_load_include('inc', 'gtm_admin', 'includes/gtm_admin.google');
  $service = _gtm_admin_get_google_gtm_service();
  if (!$service) {
    return $service;
  }

  $output = '';
  $rows = array();

  $params = array(
    'accounts' => $account_id,
    'containers' => $container_id,
  );
  $params_string = _gtm_admin_get_parent_string($params);
  $workspaces = $service->accounts_containers_workspaces->listAccountsContainersWorkspaces($params_string);

  foreach ($workspaces->getWorkspace() as $workspace) {
    $workspace_id = $workspace->getWorkspaceId();
    $workspace_name = $workspace->getName();
    $params = array(
      'accounts' => $account_id,
      'containers' => $container_id,
      'workspaces' => $workspace_id,
    );
    $params_string = _gtm_admin_get_parent_string($params);
    try {
      $status_object = $service->accounts_containers_workspaces->getStatus($params_string);
    }
    catch (Google_Service_Exception $e) {
      drupal_set_message('Google API error. Check logs for more details.', 'error');
      watchdog_exception('gtm_admin', $e);
      break;
    }

    $workspace_changes = $status_object->getWorkspaceChange();
    $actions = '';
    if (!empty($workspace_changes)) {
      $changes_names = array();
      foreach ($workspace_changes as $changes) {
        $changes_tags = $changes->getTag();
        $changes_names[] = 'Tag: ' . $changes_tags->getName();
        $workspace_changes_ids['tags'][] = $changes_tags->getTagId();
      }
      $changes_names = implode(',', $changes_names);

      $workspace_name = $workspace_name . theme('html_tag', array(
          'element' => array(
            '#tag' => 'b',
            '#value' => ' changed (' . $changes_names . ')',
          ),
        ));
      $actions = l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), array(
        'query' => array(drupal_get_destination()),
        'attributes' => array('style' => 'color: #ff0000; font-size: 150%;'),
      ));
    }

    $actions .= l('Add tag', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + array('tags' => 0)), array(
      'query' => array(drupal_get_destination()),
    ));

    $output .= theme('html_tag', array(
      'element' => array(
        '#tag' => 'div',
        '#value' => $actions,
      ),
    ));

    $params = array(
      'accounts' => $account_id,
      'containers' => $container_id,
      'workspaces' => $workspace_id,
    );
    $params_string = _gtm_admin_get_parent_string($params);
    $tags = $service->accounts_containers_workspaces_tags->listAccountsContainersWorkspacesTags($params_string);
    $cache_data[$account_id][$container_id][$workspace_id]['tags'] = $tags;

    foreach ($tags->getTag() as $tag) {
      $tag_id = $tag->getTagId();

      $tag_name = $tag->getName();
      if (in_array($tag_id, $workspace_changes_ids['tags'])) {
        $tag_name .= theme('html_tag', array(
          'element' => array(
            '#tag' => 'b',
            '#value' => ' (new)',
          ),
        ));
      }
      $rows[] = array($tag_name, _gtm_admin_get_tag_actions($params, $tag_id),
      );
    }
  }

  $header = array(
    'Tag',
    'Action',
  );

  $table = [
    'header' => $header,
    'rows' => $rows,
    'attributes' => [],
    'caption' => '',
    'colgroups' => [],
    'empty' => FALSE,
    'sticky' => FALSE,
  ];

  $output .= theme_table($table);
  return $output;
}

// Workspace page is unnecessary.
function gtm_admin_workspace_page($account_id, $container_id, $workspace_id) {
  module_load_include('inc', 'gtm_admin', 'includes/gtm_admin.google');
  $service = _gtm_admin_get_google_gtm_service();

  $rows = array();
  $output = '';

  $params = array(
    'accounts' => $account_id,
    'containers' => $container_id,
    'workspaces' => $workspace_id,
  );
  $params_string = _gtm_admin_get_parent_string($params);
  $workspace = $service->accounts_containers_workspaces->get($params_string);

  $workspace_name = $workspace->getName();
  try {
    $status_object = $service->accounts_containers_workspaces->getStatus($params_string);
  }
  catch (Google_Service_Exception $e) {
    drupal_set_message('Google API error. Check logs for more details.', 'error');
    watchdog_exception('gtm_admin', $e);
  }

  $workspace_changes = $status_object->getWorkspaceChange();
  $actions = '';
  if (!empty($workspace_changes)) {
    $changes_names = array();
    foreach ($workspace_changes as $changes) {
      $changes_tags = $changes->getTag();
      $changes_names[] = 'Tag: ' . $changes_tags->getName();
      $workspace_changes_ids['tags'][] = $changes_tags->getTagId();
    }
    $changes_names = implode(',', $changes_names);

    $workspace_name = $workspace_name . theme('html_tag', array(
        'element' => array(
          '#tag' => 'b',
          '#value' => ' changed (' . $changes_names . ')',
        ),
      ));
    $actions = l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), array(
      'query' => array(drupal_get_destination()),
      'attributes' => array('style' => 'color: #ff0000; font-size: 150%;'),
    ));
  }

  $actions .= l('Add tag', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + array('tags' => 0)), array(
    'query' => array(drupal_get_destination()),
  ));

  $output .= $actions;

  $tags = $service->accounts_containers_workspaces_tags->listAccountsContainersWorkspacesTags($params_string);
  $cache_data[$account_id][$container_id][$workspace_id]['tags'] = $tags;

  foreach ($tags->getTag() as $tag) {
    $tag_id = $tag->getTagId();

    $tag_name = $tag->getName();
    if (in_array($tag_id, $workspace_changes_ids['tags'])) {
      $tag_name .= theme('html_tag', array(
        'element' => array(
          '#tag' => 'b',
          '#value' => '*',
        ),
      ));
    }
    $rows[] = array($tag_name, _gtm_admin_get_tag_actions($params, $tag_id));
  }


  $header = array(
    'Tag',
    'Action',
  );

  $table = [
    'header' => $header,
    'rows' => $rows,
    'attributes' => [],
    'caption' => '',
    'colgroups' => [],
    'empty' => FALSE,
    'sticky' => FALSE,
  ];
  $output .= theme_table($table);

  return $output;
}

function _gtm_admin_get_tag($account_id, $container_id, $workspace_id, $tag_id) {
  module_load_include('inc', 'gtm_admin', 'includes/gtm_admin.google');
  $service = _gtm_admin_get_google_gtm_service();

  $params = array(
    'accounts' => $account_id,
    'containers' => $container_id,
    'workspaces' => $workspace_id,
    'tags' => $tag_id,
  );
  $params_string = _gtm_admin_get_parent_string($params);
  $tag = $service->accounts_containers_workspaces_tags->get($params_string);

  return $tag;
}

// TODO theme here
function _gtm_admin_get_tag_actions($params, $tag_id) {
  return l('Edit', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + array('tags' => $tag_id)), array(
    'query' => array(drupal_get_destination()),
  ))
  . ' ' .
  l('Delete', GTM_ADMIN_TAG_URL . '/delete/' . _gtm_admin_get_param_string($params + array('tags' => $tag_id)), array(
    'query' => array(drupal_get_destination()),
  ));
}

function gtm_admin_tag_page($account_id, $container_id, $workspace_id, $tag_id) {
  return '';
}
