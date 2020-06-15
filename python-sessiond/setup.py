from setuptools import setup


setup(
    name='sessiond',
    version='0.1.0',
    py_modules=['sessiond'],
    install_requires=['dbus-python'],

    description='Interface to sessiond DBus service',
    url='https://github.com/jcrd/sessiond/python-sessiond',
    license='MIT',
    author='James Reed',
    author_email='jcrd@tuta.io',

    keywords='dbus sessiond',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'License :: OSI Approved :: MIT License',
    ],
)
