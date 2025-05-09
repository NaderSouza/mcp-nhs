/*
** Zabbix
** Copyright (C) 2001-2025 Zabbix SIA
**
** Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
** documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all copies or substantial portions
** of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
** WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**/

// Package metric provides an interface for describing a schema of metric's parameters.
package metric

import (
	"encoding/json"
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"unicode"

	"golang.zabbix.com/sdk/errs"
	"golang.zabbix.com/sdk/zbxerr"
)

const (
	kindSession paramKind = iota
	kindConn
	kindGeneral
	kindSessionOnly
)

const (
	// SessionParam key name where session name will be stored during parameter evaluation.
	SessionParam = "sessionName"

	required = true
	optional = false
)

type paramKind int

// Param stores parameters' metadata.
type Param struct {
	name         string
	description  string
	kind         paramKind
	required     bool
	validator    Validator
	defaultValue *string
}

// Metric stores a description of a metric and its parameters.
type Metric struct {
	description string
	params      []*Param
	varParam    bool
}

// MetricSet stores a mapping of keys to metrics.
type MetricSet map[string]*Metric

func ucFirst(str string) string {
	for i, v := range str {
		return string(unicode.ToUpper(v)) + str[i+1:]
	}

	return ""
}

func newParam(
	name, description string,
	kind paramKind,
	required bool,
	validator Validator,
) *Param {
	name = strings.TrimSpace(name)
	if len(name) == 0 {
		panic("parameter name cannot be empty")
	}

	description = ucFirst(strings.TrimSpace(description))
	if len(description) == 0 {
		panic("parameter description cannot be empty")
	}

	if description[len(description)-1:] != "." {
		description += "."
	}

	return &Param{
		name:         name,
		description:  description,
		kind:         kind,
		required:     required,
		validator:    validator,
		defaultValue: nil,
	}
}

// NewParam creates a new parameter with given name and validator.
// Returns a pointer.
func NewParam(name, description string) *Param {
	return newParam(name, description, kindGeneral, optional, nil)
}

// NewConnParam creates a new connection parameter with given name and validator.
// Returns a pointer.
func NewConnParam(name, description string) *Param {
	return newParam(name, description, kindConn, optional, nil)
}

// NewSessionParam creates a new connection parameter with given name and validator.
// Returns a pointer.
func NewSessionOnlyParam(name, description string) *Param {
	return newParam(name, description, kindSessionOnly, optional, nil)
}

// Name returns the name of a parameter.
func (p *Param) Name() string {
	return p.name
}

// WithSession transforms a connection typed parameter to a dual purpose parameter which can be either
// a connection parameter or session name.
// Returns a pointer.
func (p *Param) WithSession() *Param {
	if p.kind != kindConn {
		panic("only connection typed parameter can be transformed to session")
	}

	p.kind = kindSession

	return p
}

// WithDefault sets the default value for a parameter.
// It panics if a default value is specified for a required parameter.
func (p *Param) WithDefault(value string) *Param {
	if p.required {
		panic("default value cannot be applied to a required parameter")
	}

	p.defaultValue = &value

	return p
}

// WithValidator sets a validator for a parameter.
func (p *Param) WithValidator(validator Validator) *Param {
	if validator == nil {
		panic("validator cannot be nil")
	}

	p.validator = validator

	if p.defaultValue != nil {
		if err := p.validator.Validate(p.defaultValue); err != nil {
			panic(fmt.Sprintf("invalid default value %q for parameter %q: %s",
				*p.defaultValue, p.name, err.Error()))
		}
	}

	return p
}

// SetRequired makes the parameter mandatory.
// It panics if default value is specified for required parameter.
func (p *Param) SetRequired() *Param {
	if p.defaultValue != nil {
		panic("required parameter cannot have a default value")
	}

	p.required = required

	return p
}

// ordinalize convert a given number to an ordinal numeral.
func ordinalize(num int) string {
	ordinals := map[int]string{
		1:  "first",
		2:  "second",
		3:  "third",
		4:  "fourth",
		5:  "fifth",
		6:  "sixth",
		7:  "seventh",
		8:  "eighth",
		9:  "ninth",
		10: "tenth",
	}

	if num >= 1 && num <= 10 {
		return ordinals[num]
	}

	suffix := "th"

	switch num % 10 {
	case 1:
		if num%100 != 11 {
			suffix = "st"
		}
	case 2:
		if num%100 != 12 {
			suffix = "nd"
		}
	case 3:
		if num%100 != 13 {
			suffix = "rd"
		}
	}

	return strconv.Itoa(num) + suffix
}

// New creates an instance of a Metric and returns a pointer to it.
// It panics if a metric is not satisfied to one of the following rules:
// 1. Parameters must be named (and names must be unique).
// 2. It's forbidden to duplicate parameters' names.
// 3. Session must be placed first.
// 4. Connection parameters must be placed in a row.
func New(description string, params []*Param, varParam bool) *Metric {
	connParamIdx := -1

	description = ucFirst(strings.TrimSpace(description))
	if len(description) == 0 {
		panic("metric description cannot be empty")
	}

	if description[len(description)-1:] != "." {
		description += "."
	}

	if params == nil {
		params = []*Param{}
	}

	if len(params) > 0 {
		if params[0].kind != kindGeneral {
			connParamIdx = 0
		}
	}

	paramsMap := make(map[string]bool)

	for i, p := range params {
		if _, exists := paramsMap[p.name]; exists {
			panic(fmt.Sprintf("name of parameter %q must be unique", p.name))
		}

		paramsMap[p.name] = true

		if i > 0 && p.kind == kindSession {
			panic("session must be placed first")
		}

		if p.kind == kindConn {
			if i-connParamIdx > 1 {
				panic(
					"parameters describing a connection must be placed in a row",
				)
			}

			connParamIdx = i
		}
	}

	return &Metric{
		description: description,
		params:      params,
		varParam:    varParam,
	}
}

func findSession(name string, sessions any) (session any) {
	v := reflect.ValueOf(sessions)
	if v.Kind() != reflect.Map {
		panic("sessions must be map of strings")
	}

	for _, key := range v.MapKeys() {
		if name == key.String() {
			session = v.MapIndex(key).Interface()
			break
		}
	}

	return
}

func mergeWithSessionData(
	out map[string]string,
	metricParams []*Param,
	session interface{},
	hc map[string]bool,
) error {
	v := reflect.ValueOf(session)
	localHC := make(map[string]bool)

	for i := 0; i < v.NumField(); i++ {
		var p *Param = nil

		val := v.Field(i).String()

		j := 0
		for j = range metricParams {
			if metricParams[j].name == v.Type().Field(i).Name {
				p = metricParams[j]
				break
			}
		}

		ordNum := ordinalize(j + 1)

		if p == nil {
			panic(
				fmt.Sprintf(
					"cannot find parameter %q in schema",
					v.Type().Field(i).Name,
				),
			)
		}

		if val == "" {
			if p.required {
				return errs.Wrapf(
					zbxerr.ErrorTooFewParameters,
					"%s parameter %q is required", ordNum, p.name,
				)
			}

			if p.defaultValue != nil {
				localHC[p.name] = true
				val = *p.defaultValue
			}
		}

		if p.validator != nil {
			if err := p.validator.Validate(&val); err != nil {
				return errs.Wrapf(
					err, "invalid %s parameter %q", ordNum, p.name,
				)
			}
		}

		if out[p.name] == "" {
			if localHC[p.name] {
				hc[p.name] = true
			}

			out[p.name] = val
		}
	}

	return nil
}

// EvalParams returns a mapping of parameters' names to their values passed to
// a plugin and/or sessions specified in the configuration file and extra
// remaining parameters. If a session is configured, then an other connection
// parameters must not be accepted and an error will be returned.
// Also it returns error in following cases:
// * incorrect number of parameters are passed;
// * missing required parameter;
// * value validation is failed.
func (m *Metric) EvalParams(
	rawParams []string, sessions any,
) (
	params map[string]string,
	extraParams []string,
	hardcodedParams map[string]bool,
	err error,
) {
	session, err := m.parseRawParams(rawParams, sessions)
	if err != nil {
		return
	}

	params = make(map[string]string)
	hardcodedParams = make(map[string]bool)

	var i int

	for _, p := range m.params {
		kind := p.kind
		if kind == kindSession {
			if session != nil {
				i++

				continue
			}

			kind = kindConn
		} else if kind == kindSessionOnly {
			continue
		}

		var val *string

		skipConnIfSessionIsSet := !(session != nil && kind == kindConn)
		ordNum := ordinalize(i + 1)

		if i >= len(rawParams) || rawParams[i] == "" {
			if p.required && skipConnIfSessionIsSet {
				return nil, nil, nil, errs.Wrapf(
					zbxerr.ErrorTooFewParameters, "%s parameter %q is required", ordNum, p.name,
				)
			}

			if p.defaultValue != nil && skipConnIfSessionIsSet {
				hardcodedParams[p.name] = true
				val = p.defaultValue
			}
		} else {
			val = &rawParams[i]
		}

		i++

		if val == nil {
			continue
		}

		if p.validator != nil && skipConnIfSessionIsSet {
			err = p.validator.Validate(val)
			if err != nil {
				return nil, nil, nil,
					errs.Wrapf(err, "invalid %s parameter %q", ordNum, p.name)
			}
		}

		if kind == kindConn || kind == kindGeneral {
			params[p.name] = *val
		}
	}

	// Fill connection parameters with data from a session.
	if session != nil {
		err = mergeWithSessionData(params, m.params, session, hardcodedParams)
		if err != nil {
			return nil, nil, nil, err
		}

		params[SessionParam] = rawParams[0]
	}

	if i < len(rawParams) {
		extraParams = rawParams[i:]
	}

	return params, extraParams, hardcodedParams, nil
}

func (m *Metric) parseRawParams(rawParams []string, sessions any) (any, error) {
	var nonsessionParams int

	for _, p := range m.params {
		if p.kind != kindSessionOnly {
			nonsessionParams++
		}
	}

	if !m.varParam && len(rawParams) > nonsessionParams {
		return nil, zbxerr.ErrorTooManyParameters
	}

	if len(rawParams) > 0 && m.params[0].kind == kindSession {
		return findSession(rawParams[0], sessions), nil
	}

	return nil, nil
}

// List returns an array of metrics' keys and their descriptions suitable to pass to plugin.RegisterMetrics.
func (ml MetricSet) List() (list []string) {
	for key, metric := range ml {
		list = append(list, key, metric.description)
	}

	return
}

func SetDefaults(
	params map[string]string, hardcoded map[string]bool, defaults any,
) error {
	def, err := sessionToMap(defaults)
	if err != nil {
		return err
	}

	setDefaults(params, hardcoded, def)

	return nil
}

func sessionToMap(session any) (map[string]string, error) {
	b, err := json.Marshal(session)
	if err != nil {
		return nil, err
	}

	out := make(map[string]string)

	err = json.Unmarshal(b, &out)
	if err != nil {
		return nil, err
	}

	return out, nil
}

func setDefaults(
	params map[string]string,
	hardcoded map[string]bool,
	defaults map[string]string,
) {
	for k, v := range defaults {
		if v != "" {
			p := params[k]
			if p == "" || hardcoded[k] {
				params[k] = v
			}
		}
	}
}
