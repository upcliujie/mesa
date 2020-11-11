import base64
import datetime
import json
import hashlib
import hmac
import os
import requests
import xml.etree.ElementTree as ET

from email.utils import formatdate
from urllib import parse

MINIO_HOST = "minio-packet.freedesktop.org"

minio_credentials = None

def sign_with_hmac(key, message):
    key = key.encode("UTF-8")
    message = message.encode("UTF-8")

    signature = hmac.new(key, message, hashlib.sha1).digest()

    return base64.encodebytes(signature).strip().decode()

def ensure_minio_credentials():
    global minio_credentials

    if minio_credentials is None:
        minio_credentials = {}

    params = {'Action': 'AssumeRoleWithWebIdentity',
              'Version': '2011-06-15',
              'RoleArn': 'arn:aws:iam::123456789012:role/FederatedWebIdentityRole',
              'RoleSessionName': '%s:%s' % (os.environ['CI_PROJECT_PATH'], os.environ['CI_JOB_ID']),
              'DurationSeconds': 900,
              'WebIdentityToken': os.environ['CI_JOB_JWT']}
    r = requests.post('https://%s' % (MINIO_HOST), params=params)
    if r.status_code >= 400:
        print(r.text)
    r.raise_for_status()

    root = ET.fromstring(r.text)
    for attr in root.iter():
        if attr.tag == '{https://sts.amazonaws.com/doc/2011-06-15/}AccessKeyId':
            minio_credentials['AccessKeyId'] = attr.text
        elif attr.tag == '{https://sts.amazonaws.com/doc/2011-06-15/}SecretAccessKey':
            minio_credentials['SecretAccessKey'] = attr.text
        elif attr.tag == '{https://sts.amazonaws.com/doc/2011-06-15/}SessionToken':
            minio_credentials['SessionToken'] = attr.text

def upload_to_minio(file_name, resource, content_type):
    ensure_minio_credentials()

    minio_key = minio_credentials['AccessKeyId']
    minio_secret = minio_credentials['SecretAccessKey']
    minio_token = minio_credentials['SessionToken']

    date = formatdate(timeval=None, localtime=False, usegmt=True)
    url = 'https://%s%s' % (MINIO_HOST, resource)
    to_sign = "PUT\n\n%s\n%s\nx-amz-security-token:%s\n%s" % (content_type, date, minio_token, resource)
    signature = sign_with_hmac(minio_secret, to_sign)

    with open(file_name, 'rb') as data:
        headers = {'Host': MINIO_HOST,
                   'Date': date,
                   'Content-Type': content_type,
                   'Authorization': 'AWS %s:%s' % (minio_key, signature),
                   'x-amz-security-token': minio_token}
        print("Uploading artifact to %s" % url);
        r = requests.put(url, headers=headers, data=data)
        if r.status_code >= 400:
            print(r.text)
        r.raise_for_status()

def upload_artifact(file_name, key, content_type):
    resource = '/artifacts/%s/%s/%s/%s' % (os.environ['CI_PROJECT_PATH'],
                                           os.environ['CI_PIPELINE_ID'],
                                           os.environ['CI_JOB_ID'],
                                           key)
    upload_to_minio(file_name, resource, content_type)
