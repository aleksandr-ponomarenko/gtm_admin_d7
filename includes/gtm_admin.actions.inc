<?php

// Add (Edit) tag form
function gtm_admin_tag_form($form, &$form_state, $account_id, $container_id, $workspace_id, $tag_id) {
  $form = [];
  $service = _gtm_admin_get_google_gtm_service();

  if (!empty($tag_id)) {
    $params = [
      'accounts' => $account_id,
      'containers' => $container_id,
      'workspaces' => $workspace_id,
      'tags' => $tag_id,
    ];
    $params_string = _gtm_admin_get_parent_string($params);
    $tag = $service->accounts_containers_workspaces_tags->get($params_string);
  }

  $form['name'] = [
    '#type' => 'textfield',
    '#title' => 'Tag name',
    '#size' => 10,
    '#maxlength' => 64,
  ];

  $form['body'] = [
    '#type' => 'textarea',
    '#title' => 'Tag content',
  ];

  if (isset($tag)) {
    $form['name']['#default_value'] = $tag->getName();
    $parameters = $tag->getParameter();
    if (isset($parameters[0])) {
      $form['body']['#default_value'] = $parameters[0]->getValue();
    }

    $form['tags'] = [
      '#type' => 'value',
      '#value' => $tag_id,
    ];
  }

  $form['accounts'] = [
    '#type' => 'value',
    '#value' => $account_id,
  ];

  $form['containers'] = [
    '#type' => 'value',
    '#value' => $container_id,
  ];

  $form['workspaces'] = [
    '#type' => 'value',
    '#value' => $workspace_id,
  ];

  $form['submit_button'] = [
    '#type' => 'submit',
    '#value' => t('Save'),
  ];

  return $form;
}

// Add (Edit) tag form submitter
function gtm_admin_tag_form_submit($form, &$form_state) {
  $service = _gtm_admin_get_google_gtm_service();

  // TODO: editable Type and Key parameters
  $tag = new Google_Service_TagManager_Tag();
  $tag->setName($form_state['values']['name']);
  $tag->setAccountId($form_state['values']['accounts']);
  $tag->setContainerId($form_state['values']['containers']);
  $tag->setWorkspaceId($form_state['values']['workspaces']);
  $tag->setType('html');
  $tag->setFiringTriggerId([GTM_ADMIN_DEFAULT_TRIGGER_ID]);
  $tag->setTagFiringOption('oncePerEvent');

  $parameter_body = new Google_Service_TagManager_Parameter();
  $parameter_body->setKey('html');
  $parameter_body->setType('template');
  $parameter_body->setValue($form_state['values']['body']);

  $tag->setParameter([$parameter_body]);

  $params = [
    'accounts' => $form_state['values']['accounts'],
    'containers' => $form_state['values']['containers'],
    'workspaces' => $form_state['values']['workspaces'],
  ];

  if (isset($form_state['values']['tags'])) {
    $params_string = _gtm_admin_get_parent_string($params + ['tags' => $form_state['values']['tags']]);
    try {
      $tag = $service->accounts_containers_workspaces_tags->update($params_string, $tag);
      drupal_set_message('Tag ' . $tag->getName() . ' was updated.');
    } catch (Google_Service_Exception $e) {
      drupal_set_message('Error updating tag. Check logs for details.', 'error');
      watchdog_exception('gtm_admin', $e);
    }
  }
  else {
    $params_string = _gtm_admin_get_parent_string($params);
    try {
      $tag = $service->accounts_containers_workspaces_tags->create($params_string, $tag);
      drupal_set_message('Tag ' . $tag->getName() . ' was created');
    } catch (Google_Service_Exception $e) {
      drupal_set_message('Error. Check logs for details.', 'error');
      watchdog_exception('gtm_admin', $e);
    }
  }

  cache_clear_all('gtm_admin_data', 'cache');
}

// Delete tag form
function gtm_admin_tag_delete_form($form, &$form_state, $account_id, $container_id, $workspace_id, $tag_id) {
  $service = _gtm_admin_get_google_gtm_service();

  $query = drupal_get_query_parameters();
  $form = [];

  $form['accounts'] = [
    '#type' => 'value',
    '#value' => $account_id,
  ];

  $form['containers'] = [
    '#type' => 'value',
    '#value' => $container_id,
  ];

  $form['workspaces'] = [
    '#type' => 'value',
    '#value' => $workspace_id,
  ];

  $form['tags'] = [
    '#type' => 'value',
    '#value' => $tag_id,
  ];

  $params = [
    'accounts' => $account_id,
    'containers' => $container_id,
    'workspaces' => $workspace_id,
    'tags' => $tag_id,
  ];
  $params_string = _gtm_admin_get_parent_string($params);
  $tag = $service->accounts_containers_workspaces_tags->get($params_string);

  $form = confirm_form($form, t('Are you sure? Tag "@name" will be deleted.', ['@name' => $tag->getName()]), $query['destination'] ? $query['destination'] : GTM_ADMIN_LISTING_PAGE_URL);

  return $form;
}

// Delete tag form submit
function gtm_admin_tag_delete_form_submit($form, &$form_state) {
  $service = _gtm_admin_get_google_gtm_service();

  $params = [
    'accounts' => $form_state['values']['accounts'],
    'containers' => $form_state['values']['containers'],
    'workspaces' => $form_state['values']['workspaces'],
    'tags' => $form_state['values']['tags'],
  ];
  $params_string = _gtm_admin_get_parent_string($params);
  try {
    $service->accounts_containers_workspaces_tags->delete($params_string);
    drupal_set_message('Tag was deleted.');
  } catch (Google_Service_Exception $e) {
    drupal_set_message('Error while deleting tag.', 'error');
  }

  cache_clear_all('gtm_admin_data', 'cache');
}

// Publish container form
function gtm_admin_workspace_publish_form($form, &$form_state, $account_id, $container_id, $workspace_id) {
  $query = drupal_get_query_parameters();
  $form = [];

  $form['accounts'] = [
    '#type' => 'value',
    '#value' => $account_id,
  ];

  $form['containers'] = [
    '#type' => 'value',
    '#value' => $container_id,
  ];

  $form['workspaces'] = [
    '#type' => 'value',
    '#value' => $workspace_id,
  ];

  $form['name'] = [
    '#type' => 'textfield',
    '#title' => 'Version name',
    '#size' => 10,
    '#maxlength' => 64,
    '#required' => TRUE,
  ];

  $form['body'] = [
    '#type' => 'textarea',
    '#title' => 'Version description',
    '#required' => TRUE,
  ];

  $form = confirm_form($form, t('Are you sure you want to publish changes?'), $query['destination'] ? $query['destination'] : GTM_ADMIN_LISTING_PAGE_URL);

  return $form;
}

// Publish container form submitter
function gtm_admin_workspace_publish_form_submit($form, &$form_state) {
  $service = _gtm_admin_get_google_gtm_service();

  $params = [
    'accounts' => $form_state['values']['accounts'],
    'containers' => $form_state['values']['containers'],
    'workspaces' => $form_state['values']['workspaces'],
  ];
  $params_string = _gtm_admin_get_parent_string($params);

  $version = new Google_Service_TagManager_CreateContainerVersionRequestVersionOptions();
  $version->setName($form_state['values']['name']);
  $version->setNotes($form_state['values']['body']);

  try {
    $version_response = $service->accounts_containers_workspaces->create_version($params_string, $version);
  } catch (Google_Service_Exception $e) {
    return;
  }

  if ($version = $version_response->getContainerVersion()) {
    $params = [
      'accounts' => $form_state['values']['accounts'],
      'containers' => $form_state['values']['containers'],
      'versions' => $version->getContainerVersionId(),
    ];
    $params_string = _gtm_admin_get_parent_string($params);

    $result = $service->accounts_containers_versions->publish($params_string);

    // TODO: after publishing new version, workspace ID has changed, for workspace page we need to get new WS version. In case we need WS page at all.
//    $container = $result->getContainerVersion();
//    $container_id = $container->getContainerId();
//
//    $params = [
//      'accounts' => $form_state['values']['accounts'],
//      'containers' => $container_id,
//    ];
//    $params_string = _gtm_admin_get_parent_string($params);
//
//    $workspaces = $service->accounts_containers_workspaces->listAccountsContainersWorkspaces($params_string);
//    $workspaces = $workspaces->getWorkspace();
//
//    if (isset($workspaces[0])) {
//      $new_workspace_id = $workspaces[0]->getWorkspaceId();
//    }

    drupal_set_message('Changes were published.');
    cache_clear_all('gtm_admin_data', 'cache');
  }
}
