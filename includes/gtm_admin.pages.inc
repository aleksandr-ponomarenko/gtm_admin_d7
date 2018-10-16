<?php

// Global list page callback
function gtm_admin_page() {
  // TODO: can we use caches safetly?
  $cache = cache_get('gtm_admin_data');
  if (!empty($cache) && !empty($cache->data)) {
    $cache_data = $cache->data;
    $loaded = 'from caches';
    $cached = TRUE;
  }
  else {
    $cache_data = [];
    $loaded = 'from API';
    $cached = FALSE;
  }

  $service = _gtm_admin_get_google_gtm_service();
  if (!$service) {
    return '';
  }

  $output = '';
  $output .= theme('html_tag', [
    'element' => [
      '#tag' => 'span',
      '#value' => t('Loaded: ' . $loaded),
    ],
  ]);

  if (isset($cache_data['accounts'])) {
    $accounts = $cache_data['accounts'];
  }
  else {
    $accounts = $service->accounts->listAccounts();
    $cache_data['accounts'] = $accounts;
  }

  foreach ($accounts->getAccount() as $account) {
    $account_id = $account->getAccountId();
    $account_h2 = theme('html_tag', [
      'element' => [
        '#tag' => 'h2',
        '#value' => $account->getName(),
      ],
    ]);
    $account_details = theme('html_tag', [
      'element' => [
        '#tag' => 'span',
        '#value' => t('Account ID: ') . $account_id,
      ],
    ]);
    $output .= theme('html_tag', [
      'element' => [
        '#tag' => 'div',
        '#value' => $account_h2 . $account_details,
      ],
    ]);

    if (isset($cache_data[$account_id]['containers'])) {
      $containers = $cache_data[$account_id]['containers'];
    }
    else {
      $params = ['accounts' => $account_id];
      $params_string = _gtm_admin_get_parent_string($params);
      $containers = $service->accounts_containers->listAccountsContainers($params_string);
      $cache_data[$account_id]['containers'] = $containers;
    }

    $header = [
      'Container',
      'Workspace',
      'Tag',
      'Action',
    ];
    $rows = [];

    foreach ($containers->getContainer() as $container) {
      $container_id = $container->getContainerId();
      $container_name = $container->getName() . ' (' . $container->getPublicId() . ')';
      $rows[] = [$container_name, '', '', ''];

      if (isset($cache_data[$account_id][$container_id]['workspaces'])) {
        $workspaces = $cache_data[$account_id][$container_id]['workspaces'];
      }
      else {
        $params = [
          'accounts' => $account->getAccountId(),
          'containers' => $container->getContainerId(),
        ];
        $params_string = _gtm_admin_get_parent_string($params);
        $workspaces = $service->accounts_containers_workspaces->listAccountsContainersWorkspaces($params_string);
        $cache_data[$account_id][$container_id]['workspaces'] = $workspaces;
      }

      $workspace_changes_ids = [];
      foreach ($workspaces->getWorkspace() as $workspace) {
        $workspace_id = $workspace->getWorkspaceId();
        $workspace_name = $workspace->getName();

        $changes_names = [];
        $workspace_changes_ids[$workspace_id] = ['tags' => []];

        $params = [
          'accounts' => $account->getAccountId(),
          'containers' => $container->getContainerId(),
          'workspaces' => $workspace_id,
        ];
        $params_string = _gtm_admin_get_parent_string($params);

        try {
          $status_object = $service->accounts_containers_workspaces->getStatus($params_string);
        } catch (Google_Service_Exception $e) {
          if ($cached) {
            cache_clear_all('gtm_admin_data', 'cache');
            drupal_set_message('Looks like GTM data was updated. Clearing caches.');
            drupal_goto(GTM_ADMIN_LISTING_PAGE_URL);
          }
          drupal_set_message('Google API error. Check logs for more details.', 'error');
          watchdog_exception('gtm_admin', $e);
          break;
        }

        $actions = '';
        $workspace_changes = $status_object->getWorkspaceChange();
        if (!empty($workspace_changes)) {
          foreach ($workspace_changes as $changes) {
            $changes_tags = $changes->getTag();
            $changes_names[] = 'Tag: ' . $changes_tags->getName();
            $workspace_changes_ids[$workspace_id]['tags'][] = $changes_tags->getTagId();
          }
          $changes_names = implode(',', $changes_names);

          $workspace_name = $workspace_name . theme('html_tag', [
              'element' => [
                '#tag' => 'b',
                '#value' => ' changed (' . $changes_names . ')',
              ],
            ]);
          $actions = l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), [
            'query' => [drupal_get_destination()],
            'attributes' => ['style' => 'color: #ff0000; font-size: 150%;'],
          ]);
        }

        $actions .= l('Add tag', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + ['tags' => 0]), [
          'query' => [drupal_get_destination()],
        ]);

        $rows[] = ['', $workspace_name, '', $actions];

        if (isset($cache_data[$account_id][$container_id][$workspace_id]['tags'])) {
          $tags = $cache_data[$account_id][$container_id][$workspace_id]['tags'];
        }
        else {
          $params = [
            'accounts' => $account->getAccountId(),
            'containers' => $container->getContainerId(),
            'workspaces' => $workspace_id,
          ];
          $params_string = _gtm_admin_get_parent_string($params);
          $tags = $service->accounts_containers_workspaces_tags->listAccountsContainersWorkspacesTags($params_string);
          $cache_data[$account_id][$container_id][$workspace_id]['tags'] = $tags;
        }

        foreach ($tags->getTag() as $tag) {
          $tag_id = $tag->getTagId();
          $tag_name = $tag->getName();

          if (in_array($tag_id, $workspace_changes_ids[$workspace_id]['tags'])) {
            $tag_name .= theme('html_tag', [
              'element' => [
                '#tag' => 'b',
                '#value' => ' (*)',
              ],
            ]);
          }
          $rows[] = [
            '',
            '',
            $tag_name,
            _gtm_admin_get_tag_actions($params, $tag_id),
          ];
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
    $output .= theme('table', $table);

  }

  // TODO: check caches use here
  //cache_set('gtm_admin_data', $cache_data, 'cache', CACHE_TEMPORARY);

  return $output;
}

// TODO: implement it if needed
// Account page callback
function gtm_admin_account_page($account_id) {

  return '';
}

// Container page callback.
// Render tag list with actions
// TODO: code deblication with gtm_admin_page - refactoring needed
function gtm_admin_container_page($account_id, $container_id, $publish_actions = TRUE) {
  $service = _gtm_admin_get_google_gtm_service();

  if (!$service) {
    return $service;
  }

  $output = '';
  $output .= theme('html_tag', [
    'element' => [
      '#tag' => 'h2',
      '#value' => t('Tags'),
    ],
  ]);

  $rows = [];

  $params = [
    'accounts' => $account_id,
    'containers' => $container_id,
  ];
  $params_string = _gtm_admin_get_parent_string($params);
  $workspaces = $service->accounts_containers_workspaces->listAccountsContainersWorkspaces($params_string);

  foreach ($workspaces->getWorkspace() as $workspace) {
    $workspace_id = $workspace->getWorkspaceId();

    $params = [
      'accounts' => $account_id,
      'containers' => $container_id,
      'workspaces' => $workspace_id,
    ];
    $params_string = _gtm_admin_get_parent_string($params);

    try {
      $status_object = $service->accounts_containers_workspaces->getStatus($params_string);
    } catch (Google_Service_Exception $e) {
      drupal_set_message('Google API error. Check logs for more details.', 'error');
      watchdog_exception('gtm_admin', $e);
      break;
    }

    $workspace_changes_ids = [];
    $workspace_changes = $status_object->getWorkspaceChange();
    $actions = '';
    $changes_names = '';
    if (!empty($workspace_changes)) {
      $changes_names = [];
      foreach ($workspace_changes as $changes) {
        $changes_tags = $changes->getTag();
        $changes_names[] = 'Tag: ' . $changes_tags->getName();
        $workspace_changes_ids['tags'][] = $changes_tags->getTagId();
      }
      $changes_names = implode(',', $changes_names);

      if ($publish_actions) {
        $actions .= l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), [
          'query' => [drupal_get_destination()],
          'attributes' => ['class' => 'button', 'style' => 'color: #ff0000; font-size: 150%;'],
        ]);
      }
    }

    $actions .= l('Add tag', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + ['tags' => 0]), [
      'query' => [drupal_get_destination()],
      'attributes' => [
        'class' => 'button',
        'style' => 'display: inline-block;',
      ],
    ]);

    if  ($publish_actions && $changes_names) {
      $actions = 'Changed: ' . $changes_names . ' ' . $actions;
    }

    $output .= theme('html_tag', [
      'element' => [
        '#tag' => 'div',
        '#value' => $actions,
      ],
    ]);

    $params = [
      'accounts' => $account_id,
      'containers' => $container_id,
      'workspaces' => $workspace_id,
    ];
    $params_string = _gtm_admin_get_parent_string($params);

    $tags = $service->accounts_containers_workspaces_tags->listAccountsContainersWorkspacesTags($params_string);
    $cache_data[$account_id][$container_id][$workspace_id]['tags'] = $tags;

    foreach ($tags->getTag() as $tag) {
      $tag_id = $tag->getTagId();
      $tag_name = $tag->getName();

      if (in_array($tag_id, $workspace_changes_ids['tags'])) {
        $tag_name .= theme('html_tag', [
          'element' => [
            '#tag' => 'b',
            '#value' => ' (*)',
          ],
        ]);
      }

      $rows[] = [
        $tag_name,
        _gtm_admin_get_tag_actions($params, $tag_id),
      ];
    }
  }

  $header = [
    'Tag',
    'Action',
  ];

  $table = [
    'header' => $header,
    'rows' => $rows,
    'attributes' => [],
    'caption' => '',
    'colgroups' => [],
    'empty' => FALSE,
    'sticky' => FALSE,
  ];

  $output .= theme('table', $table);
  return $output;
}

// Versions page callback
function gtm_admin_versions_page($account_id, $container_id) {
  $service = _gtm_admin_get_google_gtm_service();

  $output = '';

  $output .= theme('html_tag', [
    'element' => [
      '#tag' => 'h2',
      '#value' => t('Versions'),
    ],
  ]);
  $rows = [];

  $params = [
    'accounts' => $account_id,
    'containers' => $container_id,
  ];
  $params_string = _gtm_admin_get_parent_string($params);

  $versions = $service->accounts_containers_version_headers->listAccountsContainersVersionHeaders($params_string);

  $version_array = array_reverse($versions->getContainerVersionHeader());
  foreach ($version_array as $version) {
    $rows[] = [$version->getContainerVersionId(), $version->getName()];
  }

  $workspaces = $service->accounts_containers_workspaces->listAccountsContainersWorkspaces($params_string);

  foreach ($workspaces->getWorkspace() as $workspace) {
    $workspace_id = $workspace->getWorkspaceId();
    $params = [
      'accounts' => $account_id,
      'containers' => $container_id,
      'workspaces' => $workspace_id,
    ];
    $params_string = _gtm_admin_get_parent_string($params);
    $status_object = $service->accounts_containers_workspaces->getStatus($params_string);

    $workspace_changes = $status_object->getWorkspaceChange();
    $actions = '';
    if (!empty($workspace_changes)) {
      $changes_names = [];
      foreach ($workspace_changes as $changes) {
        $changes_tags = $changes->getTag();
        $changes_names[] = 'Tag: ' . $changes_tags->getName();
        $workspace_changes_ids['tags'][] = $changes_tags->getTagId();
      }
      // Tag names that was changed (just in case for now)
      $changes_names = implode(',', $changes_names);

      $actions = l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), [
        'query' => [drupal_get_destination()],
        'attributes' => [
          'class' => 'button',
          'style' => 'display: inline-block;',
        ],
      ]);
    }

    $message = theme('html_tag', [
      'element' => [
        '#tag' => 'b',
        '#value' => t('* there are new or updated tags. You need to Publish them for the changes to take effect.'),
      ],
    ]);
    array_unshift($rows, [$actions, $message]);
  }



  $header = [
    'Version ID',
    'Name',
  ];

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
  $service = _gtm_admin_get_google_gtm_service();

  $rows = [];
  $output = '';

  $params = [
    'accounts' => $account_id,
    'containers' => $container_id,
    'workspaces' => $workspace_id,
  ];
  $params_string = _gtm_admin_get_parent_string($params);
  $workspace = $service->accounts_containers_workspaces->get($params_string);

  $workspace_name = $workspace->getName();
  try {
    $status_object = $service->accounts_containers_workspaces->getStatus($params_string);
  } catch (Google_Service_Exception $e) {
    drupal_set_message('Google API error. Check logs for more details.', 'error');
    watchdog_exception('gtm_admin', $e);
  }

  $workspace_changes = $status_object->getWorkspaceChange();
  $actions = '';
  if (!empty($workspace_changes)) {
    $changes_names = [];
    foreach ($workspace_changes as $changes) {
      $changes_tags = $changes->getTag();
      $changes_names[] = 'Tag: ' . $changes_tags->getName();
      $workspace_changes_ids['tags'][] = $changes_tags->getTagId();
    }
    $changes_names = implode(',', $changes_names);

    $workspace_name = $workspace_name . theme('html_tag', [
        'element' => [
          '#tag' => 'b',
          '#value' => ' changed (' . $changes_names . ')',
        ],
      ]);
    $actions = l('Publish ', GTM_ADMIN_WORKSPACE_URL . '/publish/' . _gtm_admin_get_param_string($params), [
      'query' => [drupal_get_destination()],
      'attributes' => ['style' => 'color: #ff0000; font-size: 150%;'],
    ]);
  }

  $actions .= l('Add tag', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + ['tags' => 0]), [
    'query' => [drupal_get_destination()],
  ]);

  $output .= $actions;

  $tags = $service->accounts_containers_workspaces_tags->listAccountsContainersWorkspacesTags($params_string);
  $cache_data[$account_id][$container_id][$workspace_id]['tags'] = $tags;

  foreach ($tags->getTag() as $tag) {
    $tag_id = $tag->getTagId();

    $tag_name = $tag->getName();
    if (in_array($tag_id, $workspace_changes_ids['tags'])) {
      $tag_name .= theme('html_tag', [
        'element' => [
          '#tag' => 'b',
          '#value' => '*',
        ],
      ]);
    }
    $rows[] = [$tag_name, _gtm_admin_get_tag_actions($params, $tag_id)];
  }

  $header = [
    'Tag',
    'Action',
  ];

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
  $service = _gtm_admin_get_google_gtm_service();

  $params = [
    'accounts' => $account_id,
    'containers' => $container_id,
    'workspaces' => $workspace_id,
    'tags' => $tag_id,
  ];
  $params_string = _gtm_admin_get_parent_string($params);
  $tag = $service->accounts_containers_workspaces_tags->get($params_string);

  return $tag;
}

// TODO theme here
function _gtm_admin_get_tag_actions($params, $tag_id) {
  return l('Edit', GTM_ADMIN_TAG_URL . '/manage/' . _gtm_admin_get_param_string($params + ['tags' => $tag_id]), [
      'query' => [drupal_get_destination()],
    ])
    . ' ' .
    l('Delete', GTM_ADMIN_TAG_URL . '/delete/' . _gtm_admin_get_param_string($params + ['tags' => $tag_id]), [
      'query' => [drupal_get_destination()],
    ]);
}

// TODO: implement it if needed
// Tag page callback
function gtm_admin_tag_page($account_id, $container_id, $workspace_id, $tag_id) {
  return '';
}
