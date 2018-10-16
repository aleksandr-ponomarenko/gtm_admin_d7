<?php

function gtm_admin_config_form($form, &$form_state) {
  $form = [];

  $form['gtm_admin_settings_credentials_type'] = [
    '#type' => 'select',
    '#title' => t('Credentials type'),
    '#options' => [
      'service' => t('Service account'),
      'user' => t('User account'),
    ],
    '#default_value' => variable_get('gtm_admin_settings_credentials_type', 'service'),
    '#description' => t('What account you are using for GTM API? Service or regular user account?'),
  ];

  $form['gtm_admin_settings_credentials_file'] = [
    '#type' => 'managed_file',
    '#title' => t('Credentials JSON'),
    '#default_value' => variable_get('gtm_admin_settings_credentials_file', ''),
    '#upload_location' => 'private://gtm-admin/',
    '#upload_validators' => [
      'file_validate_extensions' => ['json'],
    ],
    '#description' => t('Put your credentials JSON file here.'),
  ];

  return system_settings_form($form);
}
